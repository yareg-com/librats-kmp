#pragma once

/**
 * @file session.h
 * @brief Post-handshake symmetric session: encrypts/decrypts transport frames.
 *
 * A Session is produced by a Handshaker once the secure channel is established.
 * The Connection holds it and runs every outbound frame through encrypt() and
 * every inbound frame through decrypt(). The plaintext mode supplies a
 * passthrough Session so the Connection code path is identical with or without
 * encryption — no `if (encrypted)` scattered through the hot path.
 */

#include "librats/util/rats_export.h"
#include "librats/core/bytes.h"
#include "librats/peer/peer_id.h"

namespace librats {

/// Upper bound on Session::overhead() over every provider (the Noise AEAD tag;
/// plaintext adds nothing). It lets code that does not hold a session yet size a
/// frame anyway — Node answers max_message_size() long before it knows which
/// connection a payload is headed for. Each provider asserts its own value
/// against this.
constexpr size_t kMaxSessionOverhead = 16;

class RATS_API Session {
public:
    virtual ~Session() = default;

    /// Encrypt `plain` into `out` (resized to fit). Returns false on failure.
    virtual bool encrypt(ByteView plain, Bytes& out) = 0;

    /// Decrypt `cipher` into `out` (resized to fit). Returns false on failure.
    virtual bool decrypt(ByteView cipher, Bytes& out) = 0;

    /// Bytes encrypt() adds to its input. Fixed per session, so a caller can size
    /// a frame before paying to encrypt it — which matters because encrypt()
    /// advances the cipher's nonce: a frame encrypted and then thrown away
    /// desynchronises the stream and the peer can no longer decrypt anything.
    /// Any check that might reject a frame therefore has to happen *first*.
    /// Never more than kMaxSessionOverhead.
    virtual size_t overhead() const = 0;

    /// The remote peer's identity, proven during the handshake.
    virtual const PeerId& remote_id() const = 0;

    /// True if traffic is actually encrypted (false for the plaintext passthrough).
    virtual bool is_secure() const = 0;
};

} // namespace librats
