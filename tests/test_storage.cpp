#include <gtest/gtest.h>
#include "librats/storage/storage.h"
#include "librats/node/node.h"
#include "librats/util/fs.h"
#include "test_paths.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#ifdef RATS_STORAGE

using namespace librats;
using namespace std::chrono_literals;

namespace {

template <typename Pred>
bool wait_for(Pred pred, std::chrono::milliseconds timeout = 15s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return pred();
}

} // namespace

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Named after the running case: TearDown deletes this directory, and ctest
        // runs the cases as concurrent processes in one working directory.
        test_dir_ = "./" + librats_test::scratch_name("test_storage_data");
        create_directories(test_dir_.c_str());
        cleanup_test_dir();
    }

    void TearDown() override {
        cleanup_test_dir();
        delete_directory(test_dir_.c_str());
    }

    void cleanup_test_dir() {
        delete_file((test_dir_ + "/rats_storage.dat").c_str());
        delete_file((test_dir_ + "/rats_storage.idx").c_str());
        delete_file((test_dir_ + "/rats_storage.dat.tmp").c_str());
    }

    std::string test_dir_;
};

//=============================================================================
// StorageEntry Tests
//=============================================================================

TEST_F(StorageTest, EntrySerializationRoundtrip) {
    StorageEntry entry;
    entry.key = "test_key";
    entry.type = StorageValueType::STRING;
    entry.data = {'h', 'e', 'l', 'l', 'o'};
    entry.timestamp_ms = 1234567890123ULL;
    entry.origin_peer_id = "peer123";
    entry.deleted = false;
    entry.calculate_checksum();

    std::vector<uint8_t> serialized = entry.serialize();
    ASSERT_GT(serialized.size(), 0);

    StorageEntry restored;
    size_t bytes_read = 0;
    bool success = StorageEntry::deserialize(serialized, 0, restored, bytes_read);

    ASSERT_TRUE(success);
    EXPECT_EQ(restored.key, entry.key);
    EXPECT_EQ(restored.type, entry.type);
    EXPECT_EQ(restored.data, entry.data);
    EXPECT_EQ(restored.timestamp_ms, entry.timestamp_ms);
    EXPECT_EQ(restored.origin_peer_id, entry.origin_peer_id);
    EXPECT_EQ(restored.deleted, entry.deleted);
    EXPECT_EQ(restored.checksum, entry.checksum);
    EXPECT_EQ(bytes_read, serialized.size());
}

TEST_F(StorageTest, EntryChecksumVerification) {
    StorageEntry entry;
    entry.key = "checksum_test";
    entry.type = StorageValueType::INT64;
    entry.data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A}; // 42 in big endian
    entry.timestamp_ms = 1000000;
    entry.origin_peer_id = "peer_abc";
    entry.calculate_checksum();

    EXPECT_TRUE(entry.verify_checksum());

    entry.data[0] = 0xFF;
    EXPECT_FALSE(entry.verify_checksum());
}

TEST_F(StorageTest, EntryLWWComparison) {
    StorageEntry older;
    older.key = "test";
    older.timestamp_ms = 1000;
    older.origin_peer_id = "peer_a";

    StorageEntry newer;
    newer.key = "test";
    newer.timestamp_ms = 2000;
    newer.origin_peer_id = "peer_b";

    EXPECT_TRUE(newer.wins_over(older));
    EXPECT_FALSE(older.wins_over(newer));

    StorageEntry same_time_a;
    same_time_a.key = "test";
    same_time_a.timestamp_ms = 1000;
    same_time_a.origin_peer_id = "peer_a";

    StorageEntry same_time_b;
    same_time_b.key = "test";
    same_time_b.timestamp_ms = 1000;
    same_time_b.origin_peer_id = "peer_b";

    EXPECT_TRUE(same_time_b.wins_over(same_time_a));
    EXPECT_FALSE(same_time_a.wins_over(same_time_b));
}

TEST_F(StorageTest, DeletedEntrySerializationRoundtrip) {
    StorageEntry entry;
    entry.key = "deleted_key";
    entry.type = StorageValueType::STRING;
    entry.data = {};
    entry.timestamp_ms = 9999999999ULL;
    entry.origin_peer_id = "deleter";
    entry.deleted = true;
    entry.calculate_checksum();

    std::vector<uint8_t> serialized = entry.serialize();

    StorageEntry restored;
    size_t bytes_read = 0;
    ASSERT_TRUE(StorageEntry::deserialize(serialized, 0, restored, bytes_read));

    EXPECT_EQ(restored.key, entry.key);
    EXPECT_TRUE(restored.deleted);
    EXPECT_TRUE(restored.data.empty());
}

//=============================================================================
// Value Type Conversion Tests
//=============================================================================

TEST_F(StorageTest, ValueTypeConversions) {
    EXPECT_EQ(storage_value_type_to_string(StorageValueType::BINARY), "binary");
    EXPECT_EQ(storage_value_type_to_string(StorageValueType::STRING), "string");
    EXPECT_EQ(storage_value_type_to_string(StorageValueType::INT64), "int64");
    EXPECT_EQ(storage_value_type_to_string(StorageValueType::DOUBLE), "double");
    EXPECT_EQ(storage_value_type_to_string(StorageValueType::JSON), "json");

    EXPECT_EQ(string_to_storage_value_type("binary"), StorageValueType::BINARY);
    EXPECT_EQ(string_to_storage_value_type("string"), StorageValueType::STRING);
    EXPECT_EQ(string_to_storage_value_type("int64"), StorageValueType::INT64);
    EXPECT_EQ(string_to_storage_value_type("double"), StorageValueType::DOUBLE);
    EXPECT_EQ(string_to_storage_value_type("json"), StorageValueType::JSON);

    EXPECT_EQ(string_to_storage_value_type("unknown"), StorageValueType::BINARY);
}

//=============================================================================
// StorageManager Basic Operations Tests (standalone, no network)
//=============================================================================

class StorageManagerTest : public StorageTest {
protected:
    void SetUp() override {
        StorageTest::SetUp();

        config_.data_directory = test_dir_;
        config_.database_name = "test_storage";
        config_.persist_to_disk = true;
        config_.enable_sync = false;  // standalone for unit tests
    }

    void TearDown() override {
        storage_.reset();

        delete_file((test_dir_ + "/test_storage.dat").c_str());
        delete_file((test_dir_ + "/test_storage.idx").c_str());
        delete_file((test_dir_ + "/test_storage.dat.tmp").c_str());

        StorageTest::TearDown();
    }

    std::unique_ptr<StorageManager> storage_;
    StorageConfig config_;
};

TEST_F(StorageManagerTest, PutAndGetString) {
    storage_ = std::make_unique<StorageManager>(config_);

    ASSERT_TRUE(storage_->put("key1", std::string("value1")));

    auto result = storage_->get_string("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value1");

    auto missing = storage_->get_string("nonexistent");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(StorageManagerTest, PutAndGetInt64) {
    storage_ = std::make_unique<StorageManager>(config_);

    ASSERT_TRUE(storage_->put("int_key", int64_t(42)));
    auto result = storage_->get_int("int_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);

    ASSERT_TRUE(storage_->put("neg_key", int64_t(-12345)));
    auto neg_result = storage_->get_int("neg_key");
    ASSERT_TRUE(neg_result.has_value());
    EXPECT_EQ(*neg_result, -12345);

    ASSERT_TRUE(storage_->put("large_key", int64_t(9223372036854775807LL)));
    auto large_result = storage_->get_int("large_key");
    ASSERT_TRUE(large_result.has_value());
    EXPECT_EQ(*large_result, 9223372036854775807LL);
}

TEST_F(StorageManagerTest, PutAndGetDouble) {
    storage_ = std::make_unique<StorageManager>(config_);

    ASSERT_TRUE(storage_->put("double_key", 3.14159265359));
    auto result = storage_->get_double("double_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 3.14159265359);

    ASSERT_TRUE(storage_->put("zero", 0.0));
    auto zero_result = storage_->get_double("zero");
    ASSERT_TRUE(zero_result.has_value());
    EXPECT_DOUBLE_EQ(*zero_result, 0.0);
}

TEST_F(StorageManagerTest, PutAndGetBinary) {
    storage_ = std::make_unique<StorageManager>(config_);

    std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0x80};
    ASSERT_TRUE(storage_->put("binary_key", binary_data));

    auto result = storage_->get_binary("binary_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, binary_data);
}

TEST_F(StorageManagerTest, PutAndGetJson) {
    storage_ = std::make_unique<StorageManager>(config_);

    librats::Json json_data = {
        {"name", "test"},
        {"count", 42},
        {"nested", {{"a", 1}, {"b", 2}}},
        {"array", {1, 2, 3, 4, 5}}
    };

    ASSERT_TRUE(storage_->put_json("json_key", json_data));

    auto result = storage_->get_json("json_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, json_data);
}

TEST_F(StorageManagerTest, TypeMismatchReturnsNullopt) {
    storage_ = std::make_unique<StorageManager>(config_);

    ASSERT_TRUE(storage_->put("str_key", std::string("hello")));

    EXPECT_FALSE(storage_->get_int("str_key").has_value());
    EXPECT_FALSE(storage_->get_double("str_key").has_value());
    EXPECT_FALSE(storage_->get_binary("str_key").has_value());
    EXPECT_FALSE(storage_->get_json("str_key").has_value());

    EXPECT_TRUE(storage_->get_string("str_key").has_value());
}

TEST_F(StorageManagerTest, DeleteKey) {
    storage_ = std::make_unique<StorageManager>(config_);

    ASSERT_TRUE(storage_->put("to_delete", std::string("value")));
    EXPECT_TRUE(storage_->has("to_delete"));

    ASSERT_TRUE(storage_->remove("to_delete"));
    EXPECT_FALSE(storage_->has("to_delete"));

    EXPECT_FALSE(storage_->remove("nonexistent"));
    EXPECT_FALSE(storage_->remove("to_delete"));
}

TEST_F(StorageManagerTest, HasKey) {
    storage_ = std::make_unique<StorageManager>(config_);

    EXPECT_FALSE(storage_->has("key"));

    ASSERT_TRUE(storage_->put("key", std::string("value")));
    EXPECT_TRUE(storage_->has("key"));

    ASSERT_TRUE(storage_->remove("key"));
    EXPECT_FALSE(storage_->has("key"));
}

TEST_F(StorageManagerTest, KeysAndSize) {
    storage_ = std::make_unique<StorageManager>(config_);

    EXPECT_EQ(storage_->size(), 0);
    EXPECT_TRUE(storage_->empty());

    ASSERT_TRUE(storage_->put("key1", std::string("v1")));
    ASSERT_TRUE(storage_->put("key2", std::string("v2")));
    ASSERT_TRUE(storage_->put("key3", std::string("v3")));

    EXPECT_EQ(storage_->size(), 3);
    EXPECT_FALSE(storage_->empty());

    auto all_keys = storage_->keys();
    EXPECT_EQ(all_keys.size(), 3);
    EXPECT_TRUE(std::find(all_keys.begin(), all_keys.end(), "key1") != all_keys.end());
    EXPECT_TRUE(std::find(all_keys.begin(), all_keys.end(), "key2") != all_keys.end());
    EXPECT_TRUE(std::find(all_keys.begin(), all_keys.end(), "key3") != all_keys.end());
}

TEST_F(StorageManagerTest, KeysWithPrefix) {
    storage_ = std::make_unique<StorageManager>(config_);

    ASSERT_TRUE(storage_->put("user:1", std::string("alice")));
    ASSERT_TRUE(storage_->put("user:2", std::string("bob")));
    ASSERT_TRUE(storage_->put("post:1", std::string("hello")));
    ASSERT_TRUE(storage_->put("post:2", std::string("world")));

    EXPECT_EQ(storage_->keys_with_prefix("user:").size(), 2);
    EXPECT_EQ(storage_->keys_with_prefix("post:").size(), 2);
    EXPECT_EQ(storage_->keys_with_prefix("comment:").size(), 0);
}

TEST_F(StorageManagerTest, Clear) {
    storage_ = std::make_unique<StorageManager>(config_);

    ASSERT_TRUE(storage_->put("k1", std::string("v1")));
    ASSERT_TRUE(storage_->put("k2", std::string("v2")));
    EXPECT_EQ(storage_->size(), 2);

    storage_->clear();
    EXPECT_EQ(storage_->size(), 0);
    EXPECT_TRUE(storage_->empty());
}

TEST_F(StorageManagerTest, UpdateExistingKey) {
    storage_ = std::make_unique<StorageManager>(config_);

    ASSERT_TRUE(storage_->put("key", std::string("original")));
    EXPECT_EQ(*storage_->get_string("key"), "original");

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_TRUE(storage_->put("key", std::string("updated")));
    EXPECT_EQ(*storage_->get_string("key"), "updated");

    EXPECT_EQ(storage_->size(), 1);
}

//=============================================================================
// Persistence Tests
//=============================================================================

TEST_F(StorageManagerTest, SaveAndLoad) {
    {
        auto storage = std::make_unique<StorageManager>(config_);

        storage->put("str", std::string("hello"));
        storage->put("int", int64_t(42));
        storage->put("dbl", 3.14);
        storage->put("bin", std::vector<uint8_t>{1, 2, 3});
        storage->put_json("json", librats::Json({{"a", 1}}));

        ASSERT_TRUE(storage->save());
    }

    {
        auto storage = std::make_unique<StorageManager>(config_);

        EXPECT_EQ(storage->size(), 5);
        EXPECT_EQ(*storage->get_string("str"), "hello");
        EXPECT_EQ(*storage->get_int("int"), 42);
        EXPECT_DOUBLE_EQ(*storage->get_double("dbl"), 3.14);
        EXPECT_EQ(*storage->get_binary("bin"), (std::vector<uint8_t>{1, 2, 3}));
        EXPECT_EQ(*storage->get_json("json"), librats::Json({{"a", 1}}));
    }
}

TEST_F(StorageManagerTest, PersistenceWithDeletedKeys) {
    {
        auto storage = std::make_unique<StorageManager>(config_);

        storage->put("keep", std::string("value"));
        storage->put("delete", std::string("gone"));
        storage->remove("delete");

        ASSERT_TRUE(storage->save());
    }

    {
        auto storage = std::make_unique<StorageManager>(config_);

        EXPECT_TRUE(storage->has("keep"));
        EXPECT_FALSE(storage->has("delete"));
        EXPECT_EQ(storage->size(), 1);
    }
}

TEST_F(StorageManagerTest, Compaction) {
    storage_ = std::make_unique<StorageManager>(config_);

    for (int i = 0; i < 10; i++) {
        storage_->put("key" + std::to_string(i), std::string("value"));
    }
    for (int i = 0; i < 5; i++) {
        storage_->remove("key" + std::to_string(i));
    }

    EXPECT_EQ(storage_->size(), 5);

    size_t removed = storage_->compact();
    EXPECT_EQ(removed, 5);
    EXPECT_EQ(storage_->size(), 5);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(StorageManagerTest, Statistics) {
    storage_ = std::make_unique<StorageManager>(config_);

    storage_->put("k1", std::string("value1"));
    storage_->put("k2", std::string("value2"));
    storage_->put("k3", std::string("value3"));
    storage_->remove("k2");

    auto stats = storage_->get_statistics();
    EXPECT_EQ(stats.total_entries, 2);
    EXPECT_EQ(stats.deleted_entries, 1);
    EXPECT_GT(stats.total_data_bytes, 0);
}

TEST_F(StorageManagerTest, StatisticsJson) {
    storage_ = std::make_unique<StorageManager>(config_);

    storage_->put("key", std::string("value"));

    auto json_stats = storage_->get_statistics_json();
    EXPECT_TRUE(json_stats.contains("total_entries"));
    EXPECT_TRUE(json_stats.contains("deleted_entries"));
    EXPECT_TRUE(json_stats.contains("total_data_bytes"));
    EXPECT_TRUE(json_stats.contains("sync_status"));

    EXPECT_EQ(json_stats["total_entries"], 1);
}

//=============================================================================
// Edge Cases Tests
//=============================================================================

TEST_F(StorageManagerTest, EmptyKeyRejected) {
    storage_ = std::make_unique<StorageManager>(config_);

    EXPECT_FALSE(storage_->put("", std::string("value")));
    EXPECT_FALSE(storage_->has(""));
}

TEST_F(StorageManagerTest, LargeValue) {
    storage_ = std::make_unique<StorageManager>(config_);

    std::string large_value(1024 * 1024, 'x');
    ASSERT_TRUE(storage_->put("large", large_value));

    auto result = storage_->get_string("large");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), large_value.size());
    EXPECT_EQ(*result, large_value);
}

TEST_F(StorageManagerTest, SpecialCharactersInKey) {
    storage_ = std::make_unique<StorageManager>(config_);

    std::string special_key = "key/with:special\nchars\t!@#$%";

    ASSERT_TRUE(storage_->put(special_key, std::string("value")));
    EXPECT_TRUE(storage_->has(special_key));
    EXPECT_EQ(*storage_->get_string(special_key), "value");
}

TEST_F(StorageManagerTest, BinaryValueWithNullBytes) {
    storage_ = std::make_unique<StorageManager>(config_);

    std::vector<uint8_t> binary_with_nulls = {0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00};
    ASSERT_TRUE(storage_->put("nullbytes", binary_with_nulls));

    auto result = storage_->get_binary("nullbytes");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, binary_with_nulls);
}

//=============================================================================
// Change Callback Tests
//=============================================================================

TEST_F(StorageManagerTest, ChangeCallback) {
    storage_ = std::make_unique<StorageManager>(config_);

    StorageChangeEvent last_event;
    int callback_count = 0;

    storage_->set_change_callback([&](const StorageChangeEvent& event) {
        last_event = event;
        callback_count++;
    });

    storage_->put("key", std::string("value"));
    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(last_event.operation, StorageOperation::OP_PUT);
    EXPECT_EQ(last_event.key, "key");
    EXPECT_EQ(last_event.type, StorageValueType::STRING);

    storage_->remove("key");
    EXPECT_EQ(callback_count, 2);
    EXPECT_EQ(last_event.operation, StorageOperation::OP_DELETE);
    EXPECT_EQ(last_event.key, "key");
}

//=============================================================================
// Network Replication Tests (two Nodes)
//=============================================================================

namespace {

NodeConfig storage_node_config(bool listen) {
    NodeConfig c;
    c.bind_address = "127.0.0.1";
    c.security = NodeConfig::Security::Noise;
    c.enable_listen = listen;
    c.protocol = librats_test::test_protocol();
    return c;
}

StorageConfig mem_storage_config() {
    StorageConfig c;
    c.persist_to_disk = false;   // memory-only: no disk side effects in this test
    c.enable_sync = true;
    return c;
}

} // namespace

class StorageReplicationTest : public ::testing::Test {};

// A put on one node propagates to a connected peer.
TEST_F(StorageReplicationTest, PutPropagatesToPeer) {
    Node server(storage_node_config(true));
    Node client(storage_node_config(false));

    auto server_store = std::make_unique<StorageManager>(mem_storage_config());
    auto client_store = std::make_unique<StorageManager>(mem_storage_config());
    StorageManager* srv = server_store.get();
    StorageManager* cli = client_store.get();

    server.add_subsystem(std::move(server_store));
    client.add_subsystem(std::move(client_store));

    ASSERT_TRUE(server.start());
    ASSERT_TRUE(client.start());

    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }))
        << "peers did not connect";

    // Write on the server; the client should receive it via replication.
    ASSERT_TRUE(srv->put("shared_key", std::string("shared_value")));
    EXPECT_TRUE(wait_for([&] {
        auto v = cli->get_string("shared_key");
        return v.has_value() && *v == "shared_value";
    })) << "put did not replicate to peer";

    // A delete also propagates.
    ASSERT_TRUE(srv->remove("shared_key"));
    EXPECT_TRUE(wait_for([&] { return !cli->has("shared_key"); })) << "delete did not replicate";

    client.stop();
    server.stop();
}

// A peer that connects later catches up via the anti-entropy snapshot exchange.
TEST_F(StorageReplicationTest, SnapshotSyncOnConnect) {
    Node server(storage_node_config(true));
    Node client(storage_node_config(false));

    auto server_store = std::make_unique<StorageManager>(mem_storage_config());
    auto client_store = std::make_unique<StorageManager>(mem_storage_config());
    StorageManager* srv = server_store.get();
    StorageManager* cli = client_store.get();

    server.add_subsystem(std::move(server_store));
    client.add_subsystem(std::move(client_store));

    ASSERT_TRUE(server.start());

    // Pre-populate the server BEFORE the client connects.
    srv->put("k1", std::string("v1"));
    srv->put("k2", int64_t(99));

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1; })) << "did not connect";

    EXPECT_TRUE(wait_for([&] {
        auto v1 = cli->get_string("k1");
        auto v2 = cli->get_int("k2");
        return v1.has_value() && *v1 == "v1" && v2.has_value() && *v2 == 99;
    })) << "client did not catch up via snapshot sync";

    client.stop();
    server.stop();
}


//=============================================================================
// Snapshot Sync: chunking, backpressure, digest and throttle
//=============================================================================

// The snapshot exchange used to be one message holding the entire store, sent on
// every connect and in both directions. Two things followed once a store grew
// past what a connection's send queue may hold: the message became unsendable,
// and the attempt cost the connection. Since the store only ever grows, a peer
// that crossed the line never got back under it -- it reconnected, tried the
// same snapshot, lost the connection again, roughly once a second, forever.
//
// These tests pin the three things that keep that from happening: chunks paced
// against the link, a digest so a converged pair carries nothing, and a floor
// between snapshots with the same peer.

namespace {

NodeConfig sync_node_config(bool listen, size_t queue_limit = 0) {
    NodeConfig c = storage_node_config(listen);
    c.send_queue_limit = queue_limit;
    return c;
}

StorageConfig sync_storage_config(uint32_t chunk_bytes = 16 * 1024,
                                  uint32_t min_interval_ms = 0) {
    StorageConfig c = mem_storage_config();
    c.sync_chunk_bytes = chunk_bytes;
    c.sync_min_interval_ms = min_interval_ms;
    return c;
}

// Distinct, incompressible-ish payload so a truncated or duplicated entry shows
// up as a value mismatch rather than passing by accident.
std::string filler(size_t n, int seed) {
    std::string s;
    s.reserve(n);
    for (size_t i = 0; i < n; i++) s.push_back(static_cast<char>('a' + ((i + seed * 7) % 26)));
    return s;
}

} // namespace

// The regression: a store far larger than the connection's send queue syncs in
// full, and the peers stay connected the whole way through. Before chunking this
// was one oversized message; the transport now refuses such a frame rather than
// closing, so without chunking the snapshot would simply never arrive -- which
// this test would catch just as surely as the reset it replaced.
TEST_F(StorageReplicationTest, SnapshotLargerThanTheSendQueueStillSyncs) {
    // Deliberately small, so "larger than the queue" is a few hundred KB and the
    // test stays fast. 64 KiB queue, 40 x 8 KiB entries = ~320 KiB of snapshot.
    constexpr size_t kQueueLimit = 64 * 1024;
    constexpr int    kEntries    = 40;
    constexpr size_t kValueSize  = 8 * 1024;

    Node server(sync_node_config(true, kQueueLimit));
    Node client(sync_node_config(false, kQueueLimit));

    auto server_store = std::make_unique<StorageManager>(sync_storage_config());
    auto client_store = std::make_unique<StorageManager>(sync_storage_config());
    StorageManager* srv = server_store.get();
    StorageManager* cli = client_store.get();

    server.add_subsystem(std::move(server_store));
    client.add_subsystem(std::move(client_store));

    ASSERT_TRUE(server.start());

    for (int i = 0; i < kEntries; i++) {
        ASSERT_TRUE(srv->put("big_key_" + std::to_string(i), filler(kValueSize, i)));
    }
    ASSERT_EQ(srv->size(), static_cast<size_t>(kEntries));

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }))
        << "peers did not connect";

    ASSERT_TRUE(wait_for([&] { return cli->size() == static_cast<size_t>(kEntries); }, 30s))
        << "snapshot did not arrive in full: got " << cli->size() << " of " << kEntries;

    // Every value byte-exact, not merely the right number of keys.
    for (int i = 0; i < kEntries; i++) {
        const auto v = cli->get_string("big_key_" + std::to_string(i));
        ASSERT_TRUE(v.has_value()) << "missing big_key_" << i;
        EXPECT_EQ(*v, filler(kValueSize, i)) << "corrupt big_key_" << i;
    }

    // And the link is still up: this is the part that used to fail.
    EXPECT_EQ(client.peer_count(), 1u) << "client lost the peer while syncing";
    EXPECT_EQ(server.peer_count(), 1u) << "server lost the peer while syncing";

    client.stop();
    server.stop();
}

// Once two stores have converged, the digest in a request means the answer
// carries no entries at all. This is what turns the steady state from "resend
// the whole store on every connect" into an exchange that costs a few hundred
// bytes -- in the observed failure every single snapshot applied 0 entries.
TEST_F(StorageReplicationTest, DigestSparesEntriesThePeerAlreadyHolds) {
    constexpr int kEntries = 25;

    Node server(sync_node_config(true));
    Node client(sync_node_config(false));

    // No throttle, so the second connect really does ask again.
    auto server_store = std::make_unique<StorageManager>(sync_storage_config(16 * 1024, 0));
    auto client_store = std::make_unique<StorageManager>(sync_storage_config(16 * 1024, 0));
    StorageManager* srv = server_store.get();
    StorageManager* cli = client_store.get();

    server.add_subsystem(std::move(server_store));
    client.add_subsystem(std::move(client_store));

    ASSERT_TRUE(server.start());
    for (int i = 0; i < kEntries; i++) {
        ASSERT_TRUE(srv->put("k" + std::to_string(i), filler(64, i)));
    }

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));
    ASSERT_TRUE(wait_for([&] { return cli->size() == static_cast<size_t>(kEntries); }, 20s))
        << "first sync did not complete";

    const uint64_t after_first = srv->get_statistics().sync_entries_sent;
    EXPECT_GE(after_first, static_cast<uint64_t>(kEntries))
        << "the first sync should have carried the whole store";

    // Reconnect. The client now holds everything the server does, and says so.
    client.stop();
    ASSERT_TRUE(wait_for([&] { return server.peer_count() == 0; }));

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));

    // Give the second exchange room to happen before judging that it carried
    // nothing -- an assertion that passes because nothing has run yet is no test.
    ASSERT_TRUE(wait_for([&] {
        return srv->get_statistics().sync_requests_received >= 2;
    }, 20s)) << "the second request never reached the server";
    std::this_thread::sleep_for(300ms);

    EXPECT_EQ(srv->get_statistics().sync_entries_sent, after_first)
        << "a converged peer must be sent no entries at all";
    EXPECT_EQ(cli->size(), static_cast<size_t>(kEntries));

    client.stop();
    server.stop();
}

// A digest must never cost a write. An entry the requester does not have, and an
// entry it has an older version of, are both sent even though the rest is
// skipped -- the digest is an optimisation, not a filter that can lose data.
TEST_F(StorageReplicationTest, DigestStillCarriesWhatThePeerIsMissingOrHasStale) {
    Node server(sync_node_config(true));
    Node client(sync_node_config(false));

    auto server_store = std::make_unique<StorageManager>(sync_storage_config(16 * 1024, 0));
    auto client_store = std::make_unique<StorageManager>(sync_storage_config(16 * 1024, 0));
    StorageManager* srv = server_store.get();
    StorageManager* cli = client_store.get();

    server.add_subsystem(std::move(server_store));
    client.add_subsystem(std::move(client_store));

    ASSERT_TRUE(server.start());
    ASSERT_TRUE(srv->put("shared", std::string("v1")));
    ASSERT_TRUE(srv->put("only_on_server", std::string("server_value")));

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));
    ASSERT_TRUE(wait_for([&] { return cli->size() == 2; }, 20s)) << "first sync did not complete";

    // Move "shared" on while the two are apart, so the client's copy goes stale.
    client.stop();
    ASSERT_TRUE(wait_for([&] { return server.peer_count() == 0; }));

    std::this_thread::sleep_for(20ms);  // a distinct LWW timestamp
    ASSERT_TRUE(srv->put("shared", std::string("v2")));
    ASSERT_TRUE(srv->put("added_while_apart", std::string("late")));

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));

    EXPECT_TRUE(wait_for([&] {
        const auto shared = cli->get_string("shared");
        return cli->size() == 3 && shared.has_value() && *shared == "v2";
    }, 20s)) << "the digest skipped an entry the peer actually needed";

    const auto late = cli->get_string("added_while_apart");
    ASSERT_TRUE(late.has_value());
    EXPECT_EQ(*late, "late");

    client.stop();
    server.stop();
}

// A peer reconnecting in a loop must not re-trigger a snapshot every time. This
// is the amplifier in the observed failure: the reconnect was ~1/s and every one
// of them asked for the store again.
TEST_F(StorageReplicationTest, RepeatedConnectsAreThrottled) {
    Node server(sync_node_config(true));
    Node client(sync_node_config(false));

    // A window far longer than the test: the second connect falls inside it.
    auto server_store = std::make_unique<StorageManager>(sync_storage_config(16 * 1024, 60000));
    auto client_store = std::make_unique<StorageManager>(sync_storage_config(16 * 1024, 60000));
    StorageManager* srv = server_store.get();
    StorageManager* cli = client_store.get();

    server.add_subsystem(std::move(server_store));
    client.add_subsystem(std::move(client_store));

    ASSERT_TRUE(server.start());
    ASSERT_TRUE(srv->put("k", std::string("v")));

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));
    ASSERT_TRUE(wait_for([&] { return cli->size() == 1; }, 20s));

    const uint64_t requests_after_first = cli->get_statistics().sync_requests_sent;
    EXPECT_GE(requests_after_first, 1u);

    // Reconnect without stopping the node, so the throttle state survives.
    {
        auto handle = client.peer(server.local_id());
        ASSERT_TRUE(handle.has_value());
        handle->disconnect();
    }
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 0; }));
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1; }));

    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(cli->get_statistics().sync_requests_sent, requests_after_first)
        << "a reconnect inside the throttle window must not ask for the store again";

    client.stop();
    server.stop();
}

// Interop: a peer that predates the digest sends a bare one-byte request. It must
// still receive the whole snapshot -- the extension lives entirely in bytes the
// old handler never read, and this is what makes it safe to deploy against peers
// already in the field.
TEST_F(StorageReplicationTest, LegacyBareSyncRequestStillGetsTheWholeSnapshot) {
    constexpr int kEntries = 12;

    Node server(sync_node_config(true));
    Node client(sync_node_config(false));

    // Throttle off and sync off on the client, so the only request in this test
    // is the hand-built legacy one below.
    StorageConfig client_cfg = sync_storage_config(16 * 1024, 0);
    client_cfg.enable_sync = false;

    auto server_store = std::make_unique<StorageManager>(sync_storage_config(16 * 1024, 0));
    auto client_store = std::make_unique<StorageManager>(client_cfg);
    StorageManager* srv = server_store.get();
    StorageManager* cli = client_store.get();

    server.add_subsystem(std::move(server_store));
    client.add_subsystem(std::move(client_store));

    ASSERT_TRUE(server.start());
    for (int i = 0; i < kEntries; i++) {
        ASSERT_TRUE(srv->put("legacy_" + std::to_string(i), filler(128, i)));
    }

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));

    // enable_sync=false means the client's StorageManager registered no handler,
    // so receive the response here instead and feed it back in by hand -- the
    // point under test is what the *server* answers a bare request with.
    std::atomic<int> entries_seen{0};
    client.on(MessageType::Storage, [&](const Peer&, ByteView payload) {
        if (payload.empty() || payload.data()[0] != 3 /* OP_SYNC_RESPONSE */) return;
        if (payload.size() < 5) return;
        const uint8_t* p = payload.data();
        const uint32_t count = (static_cast<uint32_t>(p[1]) << 24) |
                               (static_cast<uint32_t>(p[2]) << 16) |
                               (static_cast<uint32_t>(p[3]) << 8) | p[4];
        entries_seen += static_cast<int>(count);
    });

    // Exactly what a pre-digest peer puts on the wire: the opcode, nothing else.
    const std::vector<uint8_t> legacy_request{2 /* OP_SYNC_REQUEST */};
    ASSERT_TRUE(client.send(server.local_id(), MessageType::Storage, ByteView(legacy_request)));

    EXPECT_TRUE(wait_for([&] { return entries_seen.load() >= kEntries; }, 20s))
        << "a bare legacy request got " << entries_seen.load() << " of " << kEntries << " entries";
    EXPECT_EQ(cli->size(), 0u) << "the client's own store should not have been touched";

    client.stop();
    server.stop();
}

// An entry too large for any single message must not take the rest of the
// snapshot down with it. The link refuses such a frame without ever marking
// itself un-writable -- there is nothing to wait for -- so a pump that stopped
// on the refusal would wait for a wakeup that never comes, and every entry
// behind the big one would be stranded for as long as the peer stayed connected.
// So: skip it, and keep going.
TEST_F(StorageReplicationTest, OversizedEntryIsSkippedWithoutStrandingTheRest) {
    constexpr size_t kQueueLimit = 64 * 1024;
    constexpr int    kSmall      = 5;

    Node server(sync_node_config(true, kQueueLimit));
    Node client(sync_node_config(false, kQueueLimit));

    auto server_store = std::make_unique<StorageManager>(sync_storage_config());
    auto client_store = std::make_unique<StorageManager>(sync_storage_config());
    StorageManager* srv = server_store.get();
    StorageManager* cli = client_store.get();

    server.add_subsystem(std::move(server_store));
    client.add_subsystem(std::move(client_store));

    ASSERT_TRUE(server.start());
    // Well past the whole send queue, so no amount of draining could carry it.
    ASSERT_TRUE(srv->put("too_big", filler(kQueueLimit * 3, 1)));
    for (int i = 0; i < kSmall; i++) {
        ASSERT_TRUE(srv->put("small_" + std::to_string(i), filler(128, i)));
    }

    ASSERT_TRUE(client.start());
    client.connect("127.0.0.1", server.listen_port());
    ASSERT_TRUE(wait_for([&] { return client.peer_count() == 1 && server.peer_count() == 1; }));

    ASSERT_TRUE(wait_for([&] { return cli->size() == static_cast<size_t>(kSmall); }, 20s))
        << "entries behind the oversized one were stranded: got " << cli->size()
        << " of " << kSmall;

    for (int i = 0; i < kSmall; i++) {
        const auto v = cli->get_string("small_" + std::to_string(i));
        ASSERT_TRUE(v.has_value()) << "missing small_" << i;
        EXPECT_EQ(*v, filler(128, i));
    }
    EXPECT_FALSE(cli->get_string("too_big").has_value())
        << "an entry that cannot be framed must not appear to have been sent";
    EXPECT_EQ(client.peer_count(), 1u);

    client.stop();
    server.stop();
}

#endif // RATS_STORAGE
