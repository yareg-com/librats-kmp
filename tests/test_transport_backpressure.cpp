#include <gtest/gtest.h>
#include "test_paths.h"

#include "librats/node/node.h"
#include "librats/transport/connection.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace librats;
using namespace std::chrono_literals;

// Backpressure on the send path: what happens when a caller offers a connection
// more than its send queue may hold.
//
// The behaviour these tests pin used to be the opposite one. Connection::send()
// queued the frame first and only then compared the backlog against the
// high-water mark, and on finding it over the mark it *closed the connection* as
// a slow consumer. Three consequences, all of them observed in the field:
//
//   - a single message larger than the mark was unsendable by construction, yet
//     the peer was the one who paid for it — with a reset, on every attempt;
//   - the caller could not tell the teardown from ordinary congestion, because
//     both were reported as `false` from send();
//   - anything that grew a message with the data it carried (a store snapshot,
//     say) turned into a reconnect loop the moment it crossed the mark, and
//     could never get back under it.
//
// So: refuse the frame, keep the peer.

namespace {

template <typename Pred>
bool wait_for(Pred pred, std::chrono::milliseconds timeout = 10s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(1ms);
    }
    return pred();
}

// A small queue limit keeps these tests fast: the mark is what matters, not its
// size. A quarter of it is the low-water mark send() answers "no room" at.
constexpr size_t kQueueLimit = 256 * 1024;

NodeConfig backpressure_config(bool listen) {
    NodeConfig c;
    c.listen_port = 0;  // ephemeral
    c.bind_address = "127.0.0.1";
    c.security = NodeConfig::Security::Noise;
    c.enable_listen = listen;
    c.protocol = librats_test::test_protocol();
    c.send_queue_limit = kQueueLimit;
    return c;
}

std::string blob(size_t n, char fill = 'x') { return std::string(n, fill); }

} // namespace

// The regression itself: an oversized frame is refused, and the peer lives.
TEST(TransportBackpressureTest, OversizedFrameIsRefusedAndThePeerSurvives) {
    Node server(backpressure_config(true));
    Node client(backpressure_config(false));

    std::atomic<int> received{0};
    std::atomic<size_t> last_size{0};
    server.on("blob", [&](const Peer&, ByteView msg) {
        last_size = msg.size();
        received++;
    });

    ASSERT_TRUE(server.start());
    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }))
        << "peers did not connect";

    // Well past the limit, so no amount of draining could ever make it fit.
    const std::string huge = blob(kQueueLimit * 4);
    EXPECT_FALSE(client.send(server.local_id(), "blob", ByteView(huge)))
        << "an unsendable frame must be refused, not accepted";

    // The point of the whole change: the connection is still there. Give the
    // reset that used to arrive here time to show up before believing it.
    std::this_thread::sleep_for(300ms);
    EXPECT_EQ(client.peer_count(), 1u) << "client lost the peer over an oversized frame";
    EXPECT_EQ(server.peer_count(), 1u) << "server lost the peer over an oversized frame";

    // And still usable: the refusal cost one message, not the link.
    const std::string small_frame = blob(1024, 'y');
    EXPECT_TRUE(client.send(server.local_id(), "blob", ByteView(small_frame)));
    ASSERT_TRUE(wait_for([&] { return received.load() >= 1; })) << "peer no longer carries traffic";

    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(received.load(), 1) << "the refused frame must not have been delivered as well";
    EXPECT_EQ(last_size.load(), small_frame.size());

    client.stop();
    server.stop();
}

// Refusing must not cost data that was legitimately sendable: everything the
// link accepted still arrives, byte-exact, after an oversized frame is refused
// in the middle of the run.
TEST(TransportBackpressureTest, RefusalDoesNotDisturbSurroundingTraffic) {
    Node server(backpressure_config(true));
    Node client(backpressure_config(false));

    std::mutex mu;
    std::vector<std::string> got;
    server.on("seq", [&](const Peer&, ByteView msg) {
        std::lock_guard<std::mutex> l(mu);
        got.emplace_back(reinterpret_cast<const char*>(msg.data()), msg.size());
    });

    ASSERT_TRUE(server.start());
    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));

    EXPECT_TRUE(client.send(server.local_id(), "seq", ByteView(std::string("before"))));
    EXPECT_FALSE(client.send(server.local_id(), "seq", ByteView(blob(kQueueLimit * 4))));
    EXPECT_TRUE(client.send(server.local_id(), "seq", ByteView(std::string("after"))));

    ASSERT_TRUE(wait_for([&] {
        std::lock_guard<std::mutex> l(mu);
        return got.size() == 2;
    })) << "traffic either side of a refusal must still arrive";

    {
        std::lock_guard<std::mutex> l(mu);
        EXPECT_EQ(got[0], "before");
        EXPECT_EQ(got[1], "after");
    }
    EXPECT_EQ(client.peer_count(), 1u);

    client.stop();
    server.stop();
}

// A caller that keeps offering past the mark gets told to stop, keeps its peer,
// and — this is the half that makes chunking work at all — is let through again
// once the queue drains. Backpressure that never lifts is just a stall.
TEST(TransportBackpressureTest, CongestionEasesInsteadOfClosing) {
    Node server(backpressure_config(true));
    Node client(backpressure_config(false));

    std::atomic<size_t> bytes_in{0};
    server.on("bulk", [&](const Peer&, ByteView msg) { bytes_in += msg.size(); });

    ASSERT_TRUE(server.start());
    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));

    // Chunks that fit comfortably, offered far faster than the link drains, so
    // the queue is driven over its low-water mark and send() starts saying no.
    const std::string chunk = blob(16 * 1024);
    size_t offered = 0;
    bool ever_refused = false;
    for (int i = 0; i < 400; i++) {
        if (client.send(server.local_id(), "bulk", ByteView(chunk))) {
            offered += chunk.size();
        } else {
            ever_refused = true;
            break;
        }
        offered += 0;
    }
    EXPECT_TRUE(ever_refused) << "a caller outrunning the link must be told to ease off";
    EXPECT_EQ(client.peer_count(), 1u) << "congestion must not cost the peer";

    // Let it drain, then confirm the link takes traffic again.
    ASSERT_TRUE(wait_for([&] { return bytes_in.load() >= offered; }, 20s))
        << "accepted bytes did not all arrive";
    EXPECT_TRUE(wait_for([&] { return client.send(server.local_id(), "bulk", ByteView(chunk)); }))
        << "the link never became writable again";

    client.stop();
    server.stop();
}

// A frame that could never fit is a caller error, not congestion, and the two
// are answered differently: an impossible frame is refused whether the queue is
// full or empty, and refusing it does not depend on anything draining.
TEST(TransportBackpressureTest, ImpossibleFrameIsRefusedOnAnIdleLink) {
    Node server(backpressure_config(true));
    Node client(backpressure_config(false));

    ASSERT_TRUE(server.start());
    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));

    // Nothing queued: the link is as idle as it will ever be.
    ASSERT_TRUE(wait_for([&] { return client.peer_writable(server.local_id()); }));

    EXPECT_FALSE(client.send(server.local_id(), "blob", ByteView(blob(kQueueLimit + 1))));
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(client.peer_count(), 1u);
    EXPECT_EQ(server.peer_count(), 1u);

    // Still writable — a frame that never had a chance must not leave the link
    // looking congested to everything else that shares it.
    EXPECT_TRUE(wait_for([&] { return client.peer_writable(server.local_id()); }));

    client.stop();
    server.stop();
}
