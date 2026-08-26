#include "librats/storage/storage.h"
#include "librats/node/node_context.h"
#include "librats/crypto/crc32.h"
#include "librats/util/fs.h"
#include "librats/util/logger.h"
#include <algorithm>
#include <iterator>
#include <utility>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// Define logging module for this file
#define LOG_STORAGE_INFO(message) LOG_INFO("storage", message)
#define LOG_STORAGE_ERROR(message) LOG_ERROR("storage", message)
#define LOG_STORAGE_WARN(message) LOG_WARN("storage", message)
#define LOG_STORAGE_DEBUG(message) LOG_DEBUG("storage", message)

namespace librats {

namespace {

void put_u32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 3; i >= 0; --i) b.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
}

/// Overwrite the four bytes at `pos` — for a count that is only known once the
/// message it heads is complete.
void put_u32_at(std::vector<uint8_t>& b, size_t pos, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[pos + i] = static_cast<uint8_t>((v >> ((3 - i) * 8)) & 0xFF);
}

void put_u64(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 7; i >= 0; --i) b.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
}

uint32_t get_u32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |  static_cast<uint32_t>(p[3]);
}

uint64_t get_u64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}

/// FNV-1a. The digest identifies entries by hash rather than by key so that a
/// request describing a whole store stays a fixed 24 bytes per entry regardless
/// of how long the keys are. Deliberately not a cryptographic hash: at 64 bits a
/// collision between two keys held by the same pair of nodes is remote, and its
/// cost is bounded — one entry left out of one snapshot, carried by the next
/// write to it or the next sync, never a divergence that persists.
uint64_t hash64(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

} // namespace

//=============================================================================
// StorageEntry Implementation
//=============================================================================

void StorageEntry::calculate_checksum() {
    // Calculate CRC32 over key + type + data + timestamp + peer_id
    std::vector<uint8_t> buffer;

    // Add key
    buffer.insert(buffer.end(), key.begin(), key.end());

    // Add type
    buffer.push_back(static_cast<uint8_t>(type));

    // Add data
    buffer.insert(buffer.end(), data.begin(), data.end());

    // Add timestamp (8 bytes, big endian)
    for (int i = 7; i >= 0; i--) {
        buffer.push_back(static_cast<uint8_t>((timestamp_ms >> (i * 8)) & 0xFF));
    }

    // Add peer_id
    buffer.insert(buffer.end(), origin_peer_id.begin(), origin_peer_id.end());

    // Add deleted flag
    buffer.push_back(deleted ? 1 : 0);

    checksum = storage_calculate_crc32(buffer.data(), buffer.size());
}

bool StorageEntry::verify_checksum() const {
    StorageEntry temp = *this;
    temp.calculate_checksum();
    return temp.checksum == checksum;
}

std::vector<uint8_t> StorageEntry::serialize() const {
    std::vector<uint8_t> buffer;

    // Format:
    // [4 bytes] total_length (excluding this field)
    // [4 bytes] key_length
    // [key_length bytes] key
    // [1 byte] type
    // [1 byte] deleted flag
    // [8 bytes] timestamp_ms (big endian)
    // [4 bytes] peer_id_length
    // [peer_id_length bytes] origin_peer_id
    // [4 bytes] data_length
    // [data_length bytes] data
    // [4 bytes] checksum

    // Calculate total size first
    uint32_t key_len = static_cast<uint32_t>(key.size());
    uint32_t peer_id_len = static_cast<uint32_t>(origin_peer_id.size());
    uint32_t data_len = static_cast<uint32_t>(data.size());
    uint32_t total_len = 4 + key_len + 1 + 1 + 8 + 4 + peer_id_len + 4 + data_len + 4;

    buffer.reserve(4 + total_len);

    // Total length (big endian)
    buffer.push_back((total_len >> 24) & 0xFF);
    buffer.push_back((total_len >> 16) & 0xFF);
    buffer.push_back((total_len >> 8) & 0xFF);
    buffer.push_back(total_len & 0xFF);

    // Key length (big endian)
    buffer.push_back((key_len >> 24) & 0xFF);
    buffer.push_back((key_len >> 16) & 0xFF);
    buffer.push_back((key_len >> 8) & 0xFF);
    buffer.push_back(key_len & 0xFF);

    // Key
    buffer.insert(buffer.end(), key.begin(), key.end());

    // Type
    buffer.push_back(static_cast<uint8_t>(type));

    // Deleted flag
    buffer.push_back(deleted ? 1 : 0);

    // Timestamp (big endian)
    for (int i = 7; i >= 0; i--) {
        buffer.push_back(static_cast<uint8_t>((timestamp_ms >> (i * 8)) & 0xFF));
    }

    // Peer ID length (big endian)
    buffer.push_back((peer_id_len >> 24) & 0xFF);
    buffer.push_back((peer_id_len >> 16) & 0xFF);
    buffer.push_back((peer_id_len >> 8) & 0xFF);
    buffer.push_back(peer_id_len & 0xFF);

    // Peer ID
    buffer.insert(buffer.end(), origin_peer_id.begin(), origin_peer_id.end());

    // Data length (big endian)
    buffer.push_back((data_len >> 24) & 0xFF);
    buffer.push_back((data_len >> 16) & 0xFF);
    buffer.push_back((data_len >> 8) & 0xFF);
    buffer.push_back(data_len & 0xFF);

    // Data
    buffer.insert(buffer.end(), data.begin(), data.end());

    // Checksum (big endian)
    buffer.push_back((checksum >> 24) & 0xFF);
    buffer.push_back((checksum >> 16) & 0xFF);
    buffer.push_back((checksum >> 8) & 0xFF);
    buffer.push_back(checksum & 0xFF);

    return buffer;
}

bool StorageEntry::deserialize(const std::vector<uint8_t>& buffer, size_t offset,
                               StorageEntry& entry, size_t& bytes_read) {
    bytes_read = 0;

    // Minimum size check (4 bytes for total_length)
    if (offset + 4 > buffer.size()) {
        return false;
    }

    // Read total length
    uint32_t total_len = (static_cast<uint32_t>(buffer[offset]) << 24) |
                         (static_cast<uint32_t>(buffer[offset + 1]) << 16) |
                         (static_cast<uint32_t>(buffer[offset + 2]) << 8) |
                         static_cast<uint32_t>(buffer[offset + 3]);

    // Check if we have enough data
    if (offset + 4 + total_len > buffer.size()) {
        return false;
    }

    size_t pos = offset + 4;

    // Read key length
    if (pos + 4 > buffer.size()) return false;
    uint32_t key_len = (static_cast<uint32_t>(buffer[pos]) << 24) |
                       (static_cast<uint32_t>(buffer[pos + 1]) << 16) |
                       (static_cast<uint32_t>(buffer[pos + 2]) << 8) |
                       static_cast<uint32_t>(buffer[pos + 3]);
    pos += 4;

    // Read key
    if (pos + key_len > buffer.size()) return false;
    entry.key = std::string(buffer.begin() + pos, buffer.begin() + pos + key_len);
    pos += key_len;

    // Read type
    if (pos + 1 > buffer.size()) return false;
    entry.type = static_cast<StorageValueType>(buffer[pos]);
    pos += 1;

    // Read deleted flag
    if (pos + 1 > buffer.size()) return false;
    entry.deleted = buffer[pos] != 0;
    pos += 1;

    // Read timestamp
    if (pos + 8 > buffer.size()) return false;
    entry.timestamp_ms = 0;
    for (int i = 0; i < 8; i++) {
        entry.timestamp_ms = (entry.timestamp_ms << 8) | buffer[pos + i];
    }
    pos += 8;

    // Read peer ID length
    if (pos + 4 > buffer.size()) return false;
    uint32_t peer_id_len = (static_cast<uint32_t>(buffer[pos]) << 24) |
                           (static_cast<uint32_t>(buffer[pos + 1]) << 16) |
                           (static_cast<uint32_t>(buffer[pos + 2]) << 8) |
                           static_cast<uint32_t>(buffer[pos + 3]);
    pos += 4;

    // Read peer ID
    if (pos + peer_id_len > buffer.size()) return false;
    entry.origin_peer_id = std::string(buffer.begin() + pos, buffer.begin() + pos + peer_id_len);
    pos += peer_id_len;

    // Read data length
    if (pos + 4 > buffer.size()) return false;
    uint32_t data_len = (static_cast<uint32_t>(buffer[pos]) << 24) |
                        (static_cast<uint32_t>(buffer[pos + 1]) << 16) |
                        (static_cast<uint32_t>(buffer[pos + 2]) << 8) |
                        static_cast<uint32_t>(buffer[pos + 3]);
    pos += 4;

    // Read data
    if (pos + data_len > buffer.size()) return false;
    entry.data = std::vector<uint8_t>(buffer.begin() + pos, buffer.begin() + pos + data_len);
    pos += data_len;

    // Read checksum
    if (pos + 4 > buffer.size()) return false;
    entry.checksum = (static_cast<uint32_t>(buffer[pos]) << 24) |
                     (static_cast<uint32_t>(buffer[pos + 1]) << 16) |
                     (static_cast<uint32_t>(buffer[pos + 2]) << 8) |
                     static_cast<uint32_t>(buffer[pos + 3]);
    pos += 4;

    bytes_read = pos - offset;
    return true;
}

bool StorageEntry::wins_over(const StorageEntry& other) const {
    // Last-Write-Wins: compare timestamps first
    if (timestamp_ms != other.timestamp_ms) {
        return timestamp_ms > other.timestamp_ms;
    }

    // Tie-breaker: lexicographic comparison of peer IDs
    return origin_peer_id > other.origin_peer_id;
}

//=============================================================================
// Helper Functions
//=============================================================================

std::string storage_value_type_to_string(StorageValueType type) {
    switch (type) {
        case StorageValueType::BINARY: return "binary";
        case StorageValueType::STRING: return "string";
        case StorageValueType::INT64: return "int64";
        case StorageValueType::DOUBLE: return "double";
        case StorageValueType::JSON: return "json";
        default: return "unknown";
    }
}

StorageValueType string_to_storage_value_type(const std::string& str) {
    if (str == "binary") return StorageValueType::BINARY;
    if (str == "string") return StorageValueType::STRING;
    if (str == "int64") return StorageValueType::INT64;
    if (str == "double") return StorageValueType::DOUBLE;
    if (str == "json") return StorageValueType::JSON;
    return StorageValueType::BINARY;
}

//=============================================================================
// StorageManager Implementation
//=============================================================================

StorageManager::StorageManager(const StorageConfig& config)
    : config_(config),
      sync_status_(StorageSyncStatus::NOT_STARTED),
      initial_sync_complete_(false),
      running_(true),
      dirty_(false) {

    // Initialize statistics
    stats_ = StorageStatistics();
    stats_.sync_status = StorageSyncStatus::NOT_STARTED;

    initialize();
}

StorageManager::~StorageManager() {
    shutdown();
}

void StorageManager::initialize() {
    // Ensure data directory exists and load any existing data from disk.
    if (config_.persist_to_disk) {
        create_directories(config_.data_directory.c_str());
        load();
        persistence_thread_ = std::thread(&StorageManager::persistence_thread_loop, this);
    }

    LOG_STORAGE_INFO("StorageManager initialized with data directory: " << config_.data_directory);
}

void StorageManager::shutdown() {
    // Idempotent: safe to call from both stop() and the destructor.
    if (!running_.exchange(false)) return;

    LOG_STORAGE_INFO("StorageManager shutting down...");

    // Wake up persistence thread
    {
        std::lock_guard<std::mutex> lock(persistence_mutex_);
        persistence_cv_.notify_all();
    }

    // Join persistence thread
    if (persistence_thread_.joinable()) {
        persistence_thread_.join();
    }

    // Final save
    bool needs_save;
    {
        std::lock_guard<std::mutex> lock(persistence_mutex_);
        needs_save = dirty_;
    }
    if (config_.persist_to_disk && needs_save) {
        save();
    }

    LOG_STORAGE_INFO("StorageManager shut down");
}

void StorageManager::persistence_thread_loop() {
    const auto save_interval = std::chrono::seconds(5);

    while (running_.load()) {
        std::unique_lock<std::mutex> lock(persistence_mutex_);
        persistence_cv_.wait_for(lock, save_interval, [this] {
            return !running_.load() || dirty_;
        });

        if (!running_.load()) break;

        if (dirty_) {
            lock.unlock();
            save();
        }
    }
}

//=============================================================================
// Subsystem
//=============================================================================

void StorageManager::attach(NodeContext& ctx) {
    network_ = &ctx.network;

    if (!config_.enable_sync) return;

    network_->on(MessageType::Storage,
        [this](const Peer& peer, ByteView payload) { on_storage_message(peer.id(), payload); });
    network_->on_peer_connected(
        [this](const Peer& peer) { on_peer_connected(peer.id()); });
    // A snapshot leaves as a run of chunks paced against the link: send() says
    // when to stop offering, and this says when there is room again. Without the
    // second half the first half is just a stall.
    network_->on_peer_writable(
        [this](const Peer& peer) { on_peer_writable(peer.id()); });
    network_->on_peer_disconnected(
        [this](const PeerId& id) { on_peer_disconnected(id); });
}

void StorageManager::start() {
    // The persistence thread is already running (started in the constructor) and
    // sync is driven by peer events registered in attach(); nothing to spin up here.
}

void StorageManager::stop() {
    shutdown();
}

void StorageManager::set_config(const StorageConfig& config) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    config_ = config;

    if (config_.persist_to_disk) {
        create_directories(config_.data_directory.c_str());
    }
}

const StorageConfig& StorageManager::get_config() const {
    return config_;
}

//=============================================================================
// Put Operations
//=============================================================================

bool StorageManager::put(const std::string& key, const std::string& value) {
    return put_internal(key, StorageValueType::STRING, serialize_value(value));
}

bool StorageManager::put(const std::string& key, int64_t value) {
    return put_internal(key, StorageValueType::INT64, serialize_value(value));
}

bool StorageManager::put(const std::string& key, double value) {
    return put_internal(key, StorageValueType::DOUBLE, serialize_value(value));
}

bool StorageManager::put(const std::string& key, const std::vector<uint8_t>& value) {
    return put_internal(key, StorageValueType::BINARY, value);
}

bool StorageManager::put_json(const std::string& key, const librats::Json& value) {
    std::string json_str = value.dump();
    return put_internal(key, StorageValueType::JSON, serialize_value(json_str));
}

bool StorageManager::put_internal(const std::string& key, StorageValueType type,
                                  const std::vector<uint8_t>& data,
                                  uint64_t timestamp_ms,
                                  const std::string& origin_peer_id,
                                  bool broadcast) {
    if (key.empty()) {
        LOG_STORAGE_ERROR("Cannot put with empty key");
        return false;
    }

    if (data.size() > config_.max_value_size) {
        LOG_STORAGE_ERROR("Value size " << data.size() << " exceeds maximum " << config_.max_value_size);
        return false;
    }

    // Use current time if not provided
    if (timestamp_ms == 0) {
        timestamp_ms = get_current_timestamp_ms();
    }

    // Use our peer ID if not provided
    std::string peer_id = origin_peer_id.empty() ? get_our_peer_id() : origin_peer_id;

    StorageEntry new_entry(key, type, data, timestamp_ms, peer_id);

    StorageChangeEvent event;
    event.operation = StorageOperation::OP_PUT;
    event.key = key;
    event.type = type;
    event.new_data = data;
    event.timestamp_ms = timestamp_ms;
    event.origin_peer_id = peer_id;
    event.is_remote = !origin_peer_id.empty() && origin_peer_id != get_our_peer_id();

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);

        auto it = entries_.find(key);
        if (it != entries_.end()) {
            // Check LWW - only update if new entry wins
            if (!new_entry.wins_over(it->second)) {
                LOG_STORAGE_DEBUG("Rejected put for key '" << key << "' - existing entry is newer");
                return false;
            }

            event.old_data = it->second.data;
            it->second = new_entry;
        } else {
            entries_[key] = new_entry;
        }
    }

    mark_dirty();

    // Broadcast to peers if this is a local change
    if (broadcast && config_.enable_sync) {
        broadcast_entry(new_entry);
    }

    // Notify change callback
    notify_change(event);

    LOG_STORAGE_DEBUG("Put key '" << key << "' with type " << storage_value_type_to_string(type));
    return true;
}

//=============================================================================
// Get Operations
//=============================================================================

std::optional<std::string> StorageManager::get_string(const std::string& key) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    auto it = entries_.find(key);
    if (it == entries_.end() || it->second.deleted) {
        return std::nullopt;
    }

    if (it->second.type != StorageValueType::STRING) {
        return std::nullopt;
    }

    return deserialize_string(it->second.data);
}

std::optional<int64_t> StorageManager::get_int(const std::string& key) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    auto it = entries_.find(key);
    if (it == entries_.end() || it->second.deleted) {
        return std::nullopt;
    }

    if (it->second.type != StorageValueType::INT64) {
        return std::nullopt;
    }

    return deserialize_int64(it->second.data);
}

std::optional<double> StorageManager::get_double(const std::string& key) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    auto it = entries_.find(key);
    if (it == entries_.end() || it->second.deleted) {
        return std::nullopt;
    }

    if (it->second.type != StorageValueType::DOUBLE) {
        return std::nullopt;
    }

    return deserialize_double(it->second.data);
}

std::optional<std::vector<uint8_t>> StorageManager::get_binary(const std::string& key) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    auto it = entries_.find(key);
    if (it == entries_.end() || it->second.deleted) {
        return std::nullopt;
    }

    if (it->second.type != StorageValueType::BINARY) {
        return std::nullopt;
    }

    return it->second.data;
}

std::optional<librats::Json> StorageManager::get_json(const std::string& key) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    auto it = entries_.find(key);
    if (it == entries_.end() || it->second.deleted) {
        return std::nullopt;
    }

    if (it->second.type != StorageValueType::JSON) {
        return std::nullopt;
    }

    try {
        std::string json_str = deserialize_string(it->second.data);
        return librats::Json::parse(json_str);
    } catch (const std::exception& e) {
        LOG_STORAGE_ERROR("Failed to parse JSON for key '" << key << "': " << e.what());
        return std::nullopt;
    }
}

std::optional<StorageValueType> StorageManager::get_type(const std::string& key) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    auto it = entries_.find(key);
    if (it == entries_.end() || it->second.deleted) {
        return std::nullopt;
    }

    return it->second.type;
}

//=============================================================================
// Delete and Query Operations
//=============================================================================

bool StorageManager::remove(const std::string& key) {
    uint64_t timestamp_ms = get_current_timestamp_ms();
    std::string our_peer_id = get_our_peer_id();

    StorageChangeEvent event;
    event.operation = StorageOperation::OP_DELETE;
    event.key = key;
    event.timestamp_ms = timestamp_ms;
    event.origin_peer_id = our_peer_id;
    event.is_remote = false;

    StorageEntry tombstone;
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);

        auto it = entries_.find(key);
        if (it == entries_.end()) {
            return false;
        }

        if (it->second.deleted) {
            return false;  // Already deleted
        }

        event.old_data = it->second.data;

        // Mark as deleted (tombstone)
        it->second.deleted = true;
        it->second.timestamp_ms = timestamp_ms;
        it->second.origin_peer_id = our_peer_id;
        it->second.data.clear();
        it->second.calculate_checksum();
        tombstone = it->second;
    }

    mark_dirty();

    // Broadcast the tombstone to peers
    if (config_.enable_sync) {
        broadcast_entry(tombstone);
    }

    // Notify change callback
    notify_change(event);

    LOG_STORAGE_DEBUG("Deleted key '" << key << "'");
    return true;
}

bool StorageManager::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    auto it = entries_.find(key);
    return it != entries_.end() && !it->second.deleted;
}

std::vector<std::string> StorageManager::keys() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    std::vector<std::string> result;
    result.reserve(entries_.size());

    for (const auto& pair : entries_) {
        if (!pair.second.deleted) {
            result.push_back(pair.first);
        }
    }

    return result;
}

std::vector<std::string> StorageManager::keys_with_prefix(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    std::vector<std::string> result;

    for (const auto& pair : entries_) {
        if (!pair.second.deleted &&
            pair.first.size() >= prefix.size() &&
            pair.first.compare(0, prefix.size(), prefix) == 0) {
            result.push_back(pair.first);
        }
    }

    return result;
}

size_t StorageManager::size() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    size_t count = 0;
    for (const auto& pair : entries_) {
        if (!pair.second.deleted) {
            count++;
        }
    }

    return count;
}

bool StorageManager::empty() const {
    return size() == 0;
}

void StorageManager::clear() {
    std::vector<std::string> keys_to_delete;

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);

        for (auto& pair : entries_) {
            if (!pair.second.deleted) {
                keys_to_delete.push_back(pair.first);
            }
        }
    }

    for (const auto& key : keys_to_delete) {
        remove(key);
    }

    LOG_STORAGE_INFO("Cleared all entries");
}

//=============================================================================
// Persistence Operations
//=============================================================================

bool StorageManager::save() {
    if (!config_.persist_to_disk) {
        return true;
    }

    std::lock_guard<std::mutex> lock(storage_mutex_);

    bool result = write_data_file();

    if (result) {
        // dirty_ belongs to persistence_mutex_, not storage_mutex_ — the persistence
        // thread reads it under that lock. Same order as load() (storage → persistence).
        {
            std::lock_guard<std::mutex> dirty_lock(persistence_mutex_);
            dirty_ = false;
        }
        LOG_STORAGE_DEBUG("Saved " << entries_.size() << " entries to disk");
    }

    return result;
}

bool StorageManager::load() {
    if (!config_.persist_to_disk) {
        return true;
    }

    std::lock_guard<std::mutex> lock(storage_mutex_);

    std::string data_path = get_data_file_path();
    if (!file_exists(data_path.c_str())) {
        LOG_STORAGE_DEBUG("No existing data file found at " << data_path);
        return true;  // Not an error, just no data yet
    }

    bool result = read_data_file();

    if (result) {
        LOG_STORAGE_INFO("Loaded " << entries_.size() << " entries from disk");
    }

    return result;
}

size_t StorageManager::compact() {
    std::lock_guard<std::mutex> lock(storage_mutex_);

    size_t removed = 0;

    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.deleted) {
            it = entries_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        mark_dirty();
        LOG_STORAGE_INFO("Compacted storage, removed " << removed << " tombstones");
    }

    return removed;
}

//=============================================================================
// Synchronization Operations
//=============================================================================

bool StorageManager::request_sync() {
    if (!config_.enable_sync || !network_) {
        return false;
    }

    // Get a connected peer to sync from
    auto peers = network_->connected_peers();
    if (peers.empty()) {
        LOG_STORAGE_WARN("No peers available for sync");
        return false;
    }

    const PeerId& peer_id = peers[0];

    {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        sync_status_ = StorageSyncStatus::IN_PROGRESS;
    }

    send_sync_request(peer_id);

    LOG_STORAGE_INFO("Requested sync from peer " << peer_id.short_hex());
    return true;
}

StorageSyncStatus StorageManager::get_sync_status() const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    return sync_status_;
}

bool StorageManager::is_synced() const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    return initial_sync_complete_;
}

//=============================================================================
// Event Callbacks
//=============================================================================

void StorageManager::set_change_callback(StorageChangeCallback callback) {
    change_callback_ = callback;
}

void StorageManager::set_sync_complete_callback(StorageSyncCompleteCallback callback) {
    sync_complete_callback_ = callback;
}

//=============================================================================
// Statistics
//=============================================================================

StorageStatistics StorageManager::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    StorageStatistics result = stats_;

    // Calculate current counts
    {
        std::lock_guard<std::mutex> storage_lock(storage_mutex_);

        result.total_entries = 0;
        result.deleted_entries = 0;
        result.total_data_bytes = 0;

        for (const auto& pair : entries_) {
            if (pair.second.deleted) {
                result.deleted_entries++;
            } else {
                result.total_entries++;
                result.total_data_bytes += pair.second.data.size();
            }
        }
    }

    {
        std::lock_guard<std::mutex> sync_lock(sync_mutex_);
        result.sync_status = sync_status_;
        result.last_sync_time = last_sync_time_;
    }

    return result;
}

librats::Json StorageManager::get_statistics_json() const {
    StorageStatistics stats = get_statistics();

    librats::Json result;
    result["total_entries"] = stats.total_entries;
    result["deleted_entries"] = stats.deleted_entries;
    result["total_data_bytes"] = stats.total_data_bytes;
    result["disk_usage_bytes"] = stats.disk_usage_bytes;
    result["entries_synced"] = stats.entries_synced;
    result["entries_sent"] = stats.entries_sent;
    result["sync_requests_received"] = stats.sync_requests_received;
    result["sync_requests_sent"] = stats.sync_requests_sent;
    result["sync_entries_sent"] = stats.sync_entries_sent;

    switch (stats.sync_status) {
        case StorageSyncStatus::NOT_STARTED: result["sync_status"] = "not_started"; break;
        case StorageSyncStatus::IN_PROGRESS: result["sync_status"] = "in_progress"; break;
        case StorageSyncStatus::COMPLETED: result["sync_status"] = "completed"; break;
        case StorageSyncStatus::FAILED: result["sync_status"] = "failed"; break;
    }

    return result;
}

//=============================================================================
// Network Message Handlers (run on a reactor thread)
//=============================================================================

void StorageManager::on_storage_message(const PeerId& from, ByteView payload) {
    if (payload.empty()) return;

    const uint8_t* p = payload.data();
    const size_t n = payload.size();
    const uint8_t op = p[0];

    if (op == OP_ENTRY) {
        // Deserialize a single entry from the payload (after the opcode byte).
        std::vector<uint8_t> buf(p + 1, p + n);
        StorageEntry entry;
        size_t bytes_read = 0;
        if (!StorageEntry::deserialize(buf, 0, entry, bytes_read)) {
            LOG_STORAGE_WARN("Malformed storage entry from " << from.short_hex());
            return;
        }
        if (!entry.verify_checksum()) {
            LOG_STORAGE_WARN("Checksum mismatch on entry '" << entry.key << "' from " << from.short_hex());
            return;
        }

        StorageChangeEvent event;
        if (apply_remote_entry(entry, &event)) {
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.entries_synced++;
            }
            mark_dirty();
            notify_change(event);
            // Re-flood to other peers; LWW makes a duplicate lose, so this stops.
            forward_entry(entry, from);
        }
    } else if (op == OP_SYNC_REQUEST) {
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.sync_requests_received++;
        }

        // [op][version][flags][count:u32] then count x 24-byte records. Everything
        // after byte 0 is an extension: the original request was a lone opcode and
        // the original handler read no further, so a peer that predates the digest
        // simply sends one and gets the whole snapshot — which is what this falls
        // back to whenever the digest is absent, truncated or of a version we do
        // not know.
        SyncDigest digest;
        constexpr size_t kHeader = 1 + 1 + 1 + 4;
        if (n >= kHeader && p[1] == kSyncRequestVersion && (p[2] & kSyncFlagHasDigest)) {
            const uint32_t entries = get_u32(p + 3);
            // Never trust the count: only walk records the payload actually holds.
            const size_t available = (n - kHeader) / kSyncDigestRecord;
            const size_t usable = (std::min)(static_cast<size_t>(entries), available);
            if (usable < entries) {
                LOG_STORAGE_WARN("Truncated sync digest from " << from.short_hex() << ": claimed "
                                 << entries << ", carried " << available);
            }
            digest.reserve(usable);
            for (size_t i = 0; i < usable; i++) {
                const uint8_t* rec = p + kHeader + i * kSyncDigestRecord;
                digest.emplace(get_u64(rec), std::make_pair(get_u64(rec + 8), get_u64(rec + 16)));
            }
        }

        send_sync_response(from, digest);
    } else if (op == OP_SYNC_RESPONSE) {
        // [3][count:u32][entry]*
        if (n < 5) return;
        uint32_t count = (static_cast<uint32_t>(p[1]) << 24) |
                         (static_cast<uint32_t>(p[2]) << 16) |
                         (static_cast<uint32_t>(p[3]) << 8) |
                         static_cast<uint32_t>(p[4]);
        std::vector<uint8_t> buf(p + 5, p + n);
        size_t offset = 0;
        int applied = 0;
        for (uint32_t i = 0; i < count && offset < buf.size(); i++) {
            StorageEntry entry;
            size_t bytes_read = 0;
            if (!StorageEntry::deserialize(buf, offset, entry, bytes_read)) break;
            offset += bytes_read;
            if (!entry.verify_checksum()) continue;

            StorageChangeEvent event;
            if (apply_remote_entry(entry, &event)) {
                applied++;
                notify_change(event);
            }
        }

        {
            std::lock_guard<std::mutex> lock(sync_mutex_);
            sync_status_ = StorageSyncStatus::COMPLETED;
            initial_sync_complete_ = true;
            last_sync_time_ = std::chrono::steady_clock::now();
        }
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.entries_synced += applied;
        }
        if (applied > 0) mark_dirty();

        LOG_STORAGE_INFO("Sync from " << from.short_hex() << " applied " << applied << " entries");
        if (sync_complete_callback_) sync_complete_callback_(true, "");
    }
}

void StorageManager::on_peer_connected(const PeerId& peer_id) {
    if (!config_.enable_sync) return;

    // Anti-entropy: ask the new peer for a snapshot of what we are missing. Both
    // ends do this on connect, so the two databases converge via LWW.
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(peer_sync_mutex_);

        // Keep the map from growing with every peer ever seen. Anything whose
        // throttle window has long expired is carrying no information.
        if (peer_sync_.size() > kMaxTrackedPeers) {
            const auto stale = std::chrono::milliseconds(config_.sync_min_interval_ms) * 4;
            for (auto it = peer_sync_.begin(); it != peer_sync_.end();) {
                it = (it->second.queue.empty() && now - it->second.last_request > stale)
                         ? peer_sync_.erase(it) : std::next(it);
            }
        }

        PeerSyncState& st = peer_sync_[peer_id];
        if (st.last_request.time_since_epoch().count() != 0 &&
            now - st.last_request < std::chrono::milliseconds(config_.sync_min_interval_ms)) {
            // A peer reconnecting in a loop must not re-trigger a snapshot every
            // time. Live writes still reach it: gossip does not go through here.
            LOG_STORAGE_DEBUG("Skipping snapshot request to " << peer_id.short_hex()
                              << ": synced with it recently");
            return;
        }
        st.last_request = now;
    }

    {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        if (sync_status_ == StorageSyncStatus::NOT_STARTED)
            sync_status_ = StorageSyncStatus::IN_PROGRESS;
    }
    send_sync_request(peer_id);
}

void StorageManager::on_peer_writable(const PeerId& peer_id) {
    // The link has drained back under its mark, so the snapshot may go on.
    pump_sync(peer_id);
}

void StorageManager::on_peer_disconnected(const PeerId& peer_id) {
    std::lock_guard<std::mutex> lock(peer_sync_mutex_);
    auto it = peer_sync_.find(peer_id);
    if (it == peer_sync_.end()) return;

    // Drop what is still owed — the route is gone — but keep last_request, which
    // is precisely what throttles the reconnect that is probably coming next.
    it->second.queue.clear();
    it->second.next = 0;
}

//=============================================================================
// Private Methods - Serialization
//=============================================================================

std::vector<uint8_t> StorageManager::serialize_value(int64_t value) const {
    std::vector<uint8_t> data(8);
    for (int i = 7; i >= 0; i--) {
        data[7 - i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
    return data;
}

std::vector<uint8_t> StorageManager::serialize_value(double value) const {
    std::vector<uint8_t> data(8);
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(double));
    for (int i = 7; i >= 0; i--) {
        data[7 - i] = static_cast<uint8_t>((bits >> (i * 8)) & 0xFF);
    }
    return data;
}

std::vector<uint8_t> StorageManager::serialize_value(const std::string& value) const {
    return std::vector<uint8_t>(value.begin(), value.end());
}

int64_t StorageManager::deserialize_int64(const std::vector<uint8_t>& data) const {
    if (data.size() < 8) return 0;

    int64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value = (value << 8) | data[i];
    }
    return value;
}

double StorageManager::deserialize_double(const std::vector<uint8_t>& data) const {
    if (data.size() < 8) return 0.0;

    uint64_t bits = 0;
    for (int i = 0; i < 8; i++) {
        bits = (bits << 8) | data[i];
    }

    double value;
    std::memcpy(&value, &bits, sizeof(double));
    return value;
}

std::string StorageManager::deserialize_string(const std::vector<uint8_t>& data) const {
    return std::string(data.begin(), data.end());
}

//=============================================================================
// Private Methods - Network Operations
//=============================================================================

void StorageManager::broadcast_entry(const StorageEntry& entry) {
    if (!network_) return;

    std::vector<uint8_t> msg;
    msg.push_back(OP_ENTRY);
    std::vector<uint8_t> serialized = entry.serialize();
    msg.insert(msg.end(), serialized.begin(), serialized.end());

    network_->broadcast(MessageType::Storage, ByteView(msg));

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.entries_sent++;
    }
}

void StorageManager::forward_entry(const StorageEntry& entry, const PeerId& except) {
    if (!network_) return;

    std::vector<uint8_t> msg;
    msg.push_back(OP_ENTRY);
    std::vector<uint8_t> serialized = entry.serialize();
    msg.insert(msg.end(), serialized.begin(), serialized.end());
    ByteView view(msg);

    for (const PeerId& peer : network_->connected_peers()) {
        if (peer == except) continue;
        network_->send(peer, MessageType::Storage, view);
    }
}

void StorageManager::send_sync_request(const PeerId& peer_id) {
    if (!network_) return;

    // [op][version][flags][count:u32] then count x {key hash, timestamp, origin hash}.
    // Telling the peer what we already hold is what turns anti-entropy between two
    // converged stores from "send me everything" into an exchange that carries no
    // entries at all — which, after the first sync, is nearly every exchange.
    std::vector<uint8_t> msg{OP_SYNC_REQUEST, kSyncRequestVersion, 0};

    std::vector<uint8_t> digest;
    uint32_t count = 0;
    bool have_digest = false;
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        if (entries_.size() <= config_.sync_max_digest_entries) {
            digest.reserve(entries_.size() * kSyncDigestRecord);
            for (const auto& pair : entries_) {
                put_u64(digest, hash64(pair.first));
                put_u64(digest, pair.second.timestamp_ms);
                put_u64(digest, hash64(pair.second.origin_peer_id));
                count++;
            }
            have_digest = true;
        }
    }

    if (have_digest) {
        msg[2] = kSyncFlagHasDigest;
    } else {
        // Past the cap the digest would be a message in its own right. Ask without
        // one: the peer sends the whole snapshot, which is correct and — now that
        // it travels in paced chunks — no longer dangerous, merely wasteful.
        count = 0;
        digest.clear();
        LOG_STORAGE_DEBUG("Store exceeds the digest cap (" << config_.sync_max_digest_entries
                          << "); requesting a full snapshot from " << peer_id.short_hex());
    }

    put_u32(msg, count);
    msg.insert(msg.end(), digest.begin(), digest.end());

    network_->send(peer_id, MessageType::Storage, ByteView(msg));

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.sync_requests_sent++;
    }

    LOG_STORAGE_DEBUG("Sent sync request to peer " << peer_id.short_hex()
                      << " describing " << count << " entries");
}

bool StorageManager::digest_covers(const SyncDigest& digest, const StorageEntry& entry) {
    const auto it = digest.find(hash64(entry.key));
    if (it == digest.end()) return false;

    const uint64_t their_ts     = it->second.first;
    const uint64_t their_origin = it->second.second;

    // Strictly newer over there: ours would lose LWW anyway.
    if (their_ts > entry.timestamp_ms) return true;
    // Same timestamp and same origin: the very same entry.
    if (their_ts == entry.timestamp_ms && their_origin == hash64(entry.origin_peer_id)) return true;

    // Anything else — older, or a same-timestamp write from a different origin,
    // where LWW breaks the tie on the origin id — gets sent. Skipping only what
    // the peer has already shown us keeps the digest an optimisation rather than
    // a filter that decides what a peer is allowed to learn.
    return false;
}

void StorageManager::send_sync_response(const PeerId& peer_id, const SyncDigest& digest) {
    if (!network_) return;

    // Queue the keys this peer is missing; the pump turns them into chunks. The
    // snapshot is deliberately not built here: it may be far larger than what the
    // connection can hold, and materialising it whole is the very thing that made
    // a big store unsendable.
    std::vector<std::string> owed;
    size_t skipped = 0;
    {
        // Lock order is peer_sync_ then storage_, everywhere.
        std::lock_guard<std::mutex> lock(peer_sync_mutex_);
        PeerSyncState& st = peer_sync_[peer_id];

        // One snapshot at a time per peer. Answering a request is a walk of the
        // whole store plus a copy of every key it owes, so a peer that asks in a
        // loop would otherwise buy that work as often as it likes. Whatever it
        // still needs after this snapshot lands, its next request will carry.
        if (st.next < st.queue.size()) {
            LOG_STORAGE_DEBUG("Ignoring a sync request from " << peer_id.short_hex()
                              << ": a snapshot for it is still in flight");
            return;
        }

        std::lock_guard<std::mutex> slock(storage_mutex_);
        owed.reserve(entries_.size());
        for (const auto& pair : entries_) {
            if (digest_covers(digest, pair.second)) { skipped++; continue; }
            owed.push_back(pair.first);
        }
        st.queue = std::move(owed);
        st.next  = 0;
    }

    LOG_STORAGE_DEBUG("Snapshot for peer " << peer_id.short_hex() << ": " << skipped
                      << " entries already held by the requester");
    pump_sync(peer_id);
}

void StorageManager::pump_sync(const PeerId& peer_id) {
    if (!network_ || !config_.enable_sync) return;

    // A chunk is [op][count:u32] then entries. The configured budget is a target;
    // the network's cap is not, so the smaller of the two wins — a chunk built
    // past the cap would be refused outright, and the pump would then wait for a
    // writability event that is never coming.
    constexpr size_t kChunkHeader = 1 + 4;
    const size_t wanted = config_.sync_chunk_bytes ? config_.sync_chunk_bytes : 64u * 1024;
    const size_t budget = (std::min)(wanted, network_->max_message_size());
    const size_t max_entry = budget > kChunkHeader ? budget - kChunkHeader : 0;

    for (;;) {
        std::vector<uint8_t> msg;
        uint32_t count = 0;

        {
            // The cursor moves before the lock is dropped: this pump can be
            // re-entered from the writable callback on another reactor thread,
            // and two runs must never claim the same keys.
            std::lock_guard<std::mutex> lock(peer_sync_mutex_);
            auto it = peer_sync_.find(peer_id);
            if (it == peer_sync_.end()) return;
            PeerSyncState& st = it->second;

            if (st.next >= st.queue.size()) {
                if (!st.queue.empty()) {
                    LOG_STORAGE_DEBUG("Snapshot to peer " << peer_id.short_hex() << " complete");
                    st.queue.clear();
                    st.next = 0;
                }
                return;
            }

            msg.push_back(OP_SYNC_RESPONSE);
            put_u32(msg, 0);  // patched below, once the chunk is closed

            std::lock_guard<std::mutex> slock(storage_mutex_);
            while (st.next < st.queue.size()) {
                const std::string& key = st.queue[st.next];
                const auto found = entries_.find(key);
                if (found == entries_.end()) {  // removed since the snapshot began
                    st.next++;
                    continue;
                }

                const std::vector<uint8_t> serialized = found->second.serialize();
                // An entry too big for a message of its own is skipped rather than
                // offered: the link would refuse it, and stopping on that refusal
                // would strand every entry behind it too. Such an entry is simply
                // not replicable at this send-queue limit — live writes cannot
                // carry it either — so say so and move on.
                if (serialized.size() > max_entry) {
                    LOG_STORAGE_WARN("Entry '" << key << "' (" << serialized.size()
                                     << " B) exceeds what one message may carry ("
                                     << max_entry << " B); skipping it in the snapshot for "
                                     << peer_id.short_hex());
                    st.next++;
                    continue;
                }
                // Entries are not splittable, so a chunk closes *before* the entry
                // that would overrun it — that entry opens the next one. The first
                // entry always goes in: it fits the budget on its own by the check
                // above, and a chunk of nothing would make no progress.
                if (count != 0 && msg.size() + serialized.size() > budget) break;

                st.next++;
                msg.insert(msg.end(), serialized.begin(), serialized.end());
                count++;
            }

            put_u32_at(msg, 1, count);  // the count is only known now
        }

        if (count == 0) continue;  // the run held only keys that have since gone

        LOG_STORAGE_DEBUG("Sent sync chunk to peer " << peer_id.short_hex() << " with "
                          << count << " entries (" << msg.size() << " B)");
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.sync_entries_sent += count;
        }

        // The chunk is queued either way; false only means the link is at its
        // mark. Stop offering — on_peer_writable resumes from the same cursor.
        if (!network_->send(peer_id, MessageType::Storage, ByteView(msg))) return;
    }
}

bool StorageManager::apply_remote_entry(const StorageEntry& entry, StorageChangeEvent* out_event) {
    StorageChangeEvent event;
    event.operation = entry.deleted ? StorageOperation::OP_DELETE : StorageOperation::OP_PUT;
    event.key = entry.key;
    event.type = entry.type;
    event.new_data = entry.data;
    event.timestamp_ms = entry.timestamp_ms;
    event.origin_peer_id = entry.origin_peer_id;
    event.is_remote = true;

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);

        auto it = entries_.find(entry.key);
        if (it != entries_.end()) {
            // Check LWW
            if (!entry.wins_over(it->second)) {
                return false;  // Our entry is newer or identical, don't apply
            }
            event.old_data = it->second.data;
            it->second = entry;
        } else {
            entries_[entry.key] = entry;
        }
    }

    if (out_event) *out_event = event;
    return true;
}

//=============================================================================
// Private Methods - File I/O
//=============================================================================

std::string StorageManager::get_data_file_path() const {
    return combine_paths(config_.data_directory, config_.database_name + ".dat");
}

std::string StorageManager::get_index_file_path() const {
    return combine_paths(config_.data_directory, config_.database_name + ".idx");
}

bool StorageManager::write_data_file() {
    std::string data_path = get_data_file_path();
    std::string temp_path = data_path + ".tmp";

    try {
        FILE* file = fopen(temp_path.c_str(), "wb");
        if (!file) {
            LOG_STORAGE_ERROR("Failed to open temp file for writing: " << temp_path);
            return false;
        }

        // Write file header
        // Magic: "RATS" (4 bytes)
        // Version: 1 (4 bytes)
        // Entry count (4 bytes)
        const char* magic = "RATS";
        uint32_t version = 1;
        uint32_t entry_count = static_cast<uint32_t>(entries_.size());

        fwrite(magic, 1, 4, file);

        uint8_t version_bytes[4] = {
            static_cast<uint8_t>((version >> 24) & 0xFF),
            static_cast<uint8_t>((version >> 16) & 0xFF),
            static_cast<uint8_t>((version >> 8) & 0xFF),
            static_cast<uint8_t>(version & 0xFF)
        };
        fwrite(version_bytes, 1, 4, file);

        uint8_t count_bytes[4] = {
            static_cast<uint8_t>((entry_count >> 24) & 0xFF),
            static_cast<uint8_t>((entry_count >> 16) & 0xFF),
            static_cast<uint8_t>((entry_count >> 8) & 0xFF),
            static_cast<uint8_t>(entry_count & 0xFF)
        };
        fwrite(count_bytes, 1, 4, file);

        // Write each entry
        for (const auto& pair : entries_) {
            std::vector<uint8_t> serialized = pair.second.serialize();
            fwrite(serialized.data(), 1, serialized.size(), file);
        }

        fclose(file);

        // On Windows, rename fails if destination exists, so delete it first
        if (file_exists(data_path.c_str())) {
            delete_file(data_path.c_str());
        }

        // Atomically rename temp file to final
        if (!rename_file(temp_path.c_str(), data_path.c_str())) {
            LOG_STORAGE_ERROR("Failed to rename temp file to final: " << temp_path << " -> " << data_path);
            delete_file(temp_path.c_str());
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        LOG_STORAGE_ERROR("Exception while writing data file: " << e.what());
        delete_file(temp_path.c_str());
        return false;
    }
}

bool StorageManager::read_data_file() {
    std::string data_path = get_data_file_path();

    try {
        size_t file_size;
        void* file_data = read_file_binary(data_path.c_str(), &file_size);
        if (!file_data) {
            LOG_STORAGE_ERROR("Failed to read data file: " << data_path);
            return false;
        }

        std::vector<uint8_t> buffer(static_cast<uint8_t*>(file_data),
                                    static_cast<uint8_t*>(file_data) + file_size);
        free_file_buffer(file_data);

        // Read header
        if (buffer.size() < 12) {
            LOG_STORAGE_ERROR("Data file too small: " << buffer.size());
            return false;
        }

        // Check magic
        if (buffer[0] != 'R' || buffer[1] != 'A' || buffer[2] != 'T' || buffer[3] != 'S') {
            LOG_STORAGE_ERROR("Invalid magic in data file");
            return false;
        }

        // Read version
        uint32_t version = (static_cast<uint32_t>(buffer[4]) << 24) |
                          (static_cast<uint32_t>(buffer[5]) << 16) |
                          (static_cast<uint32_t>(buffer[6]) << 8) |
                          static_cast<uint32_t>(buffer[7]);

        if (version != 1) {
            LOG_STORAGE_ERROR("Unsupported data file version: " << version);
            return false;
        }

        // Read entry count
        uint32_t entry_count = (static_cast<uint32_t>(buffer[8]) << 24) |
                              (static_cast<uint32_t>(buffer[9]) << 16) |
                              (static_cast<uint32_t>(buffer[10]) << 8) |
                              static_cast<uint32_t>(buffer[11]);

        // Read entries
        entries_.clear();
        size_t offset = 12;

        for (uint32_t i = 0; i < entry_count && offset < buffer.size(); i++) {
            StorageEntry entry;
            size_t bytes_read = 0;

            if (!StorageEntry::deserialize(buffer, offset, entry, bytes_read)) {
                LOG_STORAGE_ERROR("Failed to deserialize entry " << i << " at offset " << offset);
                return false;
            }

            // Verify checksum
            if (!entry.verify_checksum()) {
                LOG_STORAGE_WARN("Checksum mismatch for entry '" << entry.key << "', skipping");
                offset += bytes_read;
                continue;
            }

            entries_[entry.key] = entry;
            offset += bytes_read;
        }

        return true;

    } catch (const std::exception& e) {
        LOG_STORAGE_ERROR("Exception while reading data file: " << e.what());
        return false;
    }
}

//=============================================================================
// Private Methods - Utility
//=============================================================================

uint64_t StorageManager::get_current_timestamp_ms() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string StorageManager::get_our_peer_id() const {
    if (!network_) return "";
    return network_->local_id().to_hex();
}

void StorageManager::notify_change(const StorageChangeEvent& event) {
    if (change_callback_) {
        change_callback_(event);
    }
}

void StorageManager::mark_dirty() {
    {
        std::lock_guard<std::mutex> lock(persistence_mutex_);
        dirty_ = true;
    }
    persistence_cv_.notify_one();
}

} // namespace librats
