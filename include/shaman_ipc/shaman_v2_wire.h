// SHAMAN v2 wire format: the shared-memory layout and protocol helpers that Orange (writer)
// and Citrus (reader) must agree on byte for byte. Layout revision: kSchemaVersion below.
// Field meanings for the frame ids: agent-contracts citrus-orange/frame_identity_v1.
// Any change to a struct, constant or helper here is a new tagged release of this repo.
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <type_traits>

namespace shaman_v2 {

inline constexpr uint64_t kMagic = 0x4f524e4753484d32ULL; // ORNGSHM2
// Protocol generation remains Shaman-v2. Revision 4 adds grouped-live
// observation provenance and producer restart identity. slot_bytes/queue_bytes
// make older shared-memory objects fail closed instead of being reinterpreted.
inline constexpr uint32_t kSchemaVersion = 4;
inline constexpr uint32_t kQueueSize = 64;
inline constexpr uint32_t kMaxObjects = 64;
inline constexpr uint32_t kMaxKeypointsPerObject = 32;
inline constexpr const char* kIpcGroup = "ipc";
inline constexpr mode_t kIpcMode = 0660;

enum class PayloadKind : uint32_t {
    kLatestTrackingState = 1,
    kStreamStatus = 2,
};

enum class DetectionStatus : uint32_t {
    kDisabled = 0,
    kNotScheduled = 1,
    kPending = 2,
    kDetections = 3,
    kZeroDetections = 4,
    kFailed = 5,
};

enum class PoseStatus : uint32_t {
    kDisabled = 0,
    kNotRequested = 1,
    kPending = 2,
    kPoses = 3,
    kNoResult = 4,
    kFailed = 5,
};

// A latest-state object vector has no stable cross-frame ordering. The value
// is carried in every slot so consumers cannot accidentally treat payload
// index as a track, region, or scientific fish identity.
enum class ObjectOrder : uint32_t {
    kUnspecified = 0,
    kUnorderedPayloadLocal = 1,
};

inline constexpr uint32_t kObjectOrderUnorderedPayloadLocal =
    static_cast<uint32_t>(ObjectOrder::kUnorderedPayloadLocal);

// Stable, numeric reasons for terminal/exceptional detection outcomes. These
// values are ABI, not display strings; descriptions belong in the contract.
enum class DetectionResultReason : uint32_t {
    kNone = 0,
    kNoSourceDetections = 1,
    kAllDetectionsRejectedByMask = 2,
    kObjectsTruncated = 3,
    kYoloWorkerEnqueueRejected = 4,
    kInferenceTimeout = 5,
    kCpuResultsSkipped = 6,
    kProcessingFailed = 7,
};

enum ObjectFlags : uint32_t {
    kObjectHasBbox = 1u << 0,
    kObjectHasPose = 1u << 1,
    kObjectSynthetic = 1u << 2,
};

enum KeypointFlags : uint16_t {
    kKeypointVisible = 1u << 0,
    kKeypointInterpolated = 1u << 1,
};

struct Keypoint {
    float x_px = 0.0f;
    float y_px = 0.0f;
    float confidence = 0.0f;
    uint16_t label_id = 0;
    uint16_t flags = 0;
};

struct Object {
    float x_px = 0.0f;
    float y_px = 0.0f;
    float width_px = 0.0f;
    float height_px = 0.0f;
    float confidence = 0.0f;
    int32_t label_id = 0;
    int32_t track_id = -1;
    uint32_t flags = 0;
    uint32_t keypoint_count = 0;
    Keypoint keypoints[kMaxKeypointsPerObject]{};
};

struct Slot {
    uint64_t magic = kMagic;
    uint32_t schema_version = kSchemaVersion;
    uint32_t slot_bytes = sizeof(Slot);

    uint64_t sequence_id = 0;
    uint32_t payload_kind = static_cast<uint32_t>(PayloadKind::kLatestTrackingState);
    uint32_t flags = 0;

    // Frame identity, per agent-contracts citrus-orange/frame_identity_v1_2026-10-12.md.
    // The two *_stream_frame_id fields are values of the Orange stream counter: frames received
    // since this camera's stream opened (preview included), 1-based, never reset at Record.
    //   state: the frame this slot's live state is current for (never moves backward).
    //   source: the frame whose detection/pose produced the content (stale if < state).
    // camera_frame_id: the camera's own 16-bit counter (wraps 65535 -> 1); never an index.
    // recording_frame_id: 1-based from Record, continuous across clips, 0 outside recording;
    //   the only key for joining to Orange's videos and metadata.
    // Renamed from state_frame_id / source_frame_id on 2026-10-12: names only, same offsets and
    // meaning, so kSchemaVersion and the layout asserts below are unchanged.
    uint64_t state_stream_frame_id = 0;
    uint64_t source_stream_frame_id = 0;
    uint64_t camera_frame_id = 0;
    uint64_t recording_frame_id = 0;
    uint64_t camera_timestamp_ns = 0;
    uint64_t timestamp_sys_ns = 0;
    uint64_t orange_publish_timestamp_us_epoch = 0;
    uint64_t orange_publish_timestamp_us_monotonic = 0;

    // Empty iff recording_frame_id is zero. Otherwise this is the exact
    // sha256:<hex> token for recording_session.json.session_id.
    char recording_identity_token[72]{};

    uint32_t camera_id = 0;
    char camera_serial[32]{};
    uint32_t source_width_px = 0;
    uint32_t source_height_px = 0;

    uint32_t detection_status = static_cast<uint32_t>(DetectionStatus::kDisabled);
    uint32_t pose_status = static_cast<uint32_t>(PoseStatus::kDisabled);
    uint64_t detection_model_id_hash = 0;
    uint64_t pose_model_id_hash = 0;
    uint64_t pose_skeleton_id_hash = 0;

    uint32_t object_count = 0;
    Object objects[kMaxObjects]{};

    // Grouped-live observation metadata is append-only after the v3 object
    // vector. source_detection_count is the detector/post-NMS count before
    // the outer spatial mask; retained_detection_count is the post-mask
    // vector count. transmitted_object_count is the number actually present
    // in objects[]; object_count remains its v3 compatibility alias.
    // objects_truncated is the number omitted due to SHAMAN capacity. A
    // nonzero truncation count requires failed status.
    uint32_t source_detection_count = 0;
    uint32_t retained_detection_count = 0;
    uint32_t transmitted_object_count = 0;
    uint32_t objects_truncated = 0;
    uint32_t object_order = kObjectOrderUnorderedPayloadLocal;
    uint32_t detection_reason =
        static_cast<uint32_t>(DetectionResultReason::kNone);

    // Producer generation changes on every authoritative writer restart.
    // producer_instance_id identifies this particular writer process/run.
    uint64_t producer_generation = 0;
    uint64_t producer_instance_id = 0;
};

struct SharedQueue {
    uint64_t magic = kMagic;
    uint32_t schema_version = kSchemaVersion;
    uint32_t queue_bytes = sizeof(SharedQueue);
    uint32_t slot_bytes = sizeof(Slot);
    uint32_t queue_size = kQueueSize;
    std::atomic<bool> initialized{false};
    std::atomic<uint64_t> writer_sequence{0};
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    std::atomic<uint64_t> push_failures{0};
    std::atomic<uint64_t> stale_suppressed{0};
    Slot queue[kQueueSize]{};

    // Appended after the v3 queue ring. These atomics let a reader observe a
    // writer reset without interpreting a predecessor generation as current.
    std::atomic<uint64_t> producer_generation{0};
    std::atomic<uint64_t> producer_instance_id{0};
};

static_assert(sizeof(Keypoint) == 16, "Shaman v2 keypoint ABI changed");
static_assert(sizeof(Object) == 548, "Shaman v2 object ABI changed");
static_assert(sizeof(Slot) == 35368, "Shaman v2 slot ABI changed");
static_assert(sizeof(SharedQueue) == 2263640, "Shaman v2 queue ABI changed");
static_assert(std::is_standard_layout<Keypoint>::value, "Keypoint must be ABI-stable");
static_assert(std::is_standard_layout<Object>::value, "Object must be ABI-stable");
static_assert(std::is_standard_layout<Slot>::value, "Slot must be ABI-stable");

// Every field's byte offset, generated from the layout Orange and Citrus shared at revision 4
// (2026-10-12). A change here is a layout change: new kSchemaVersion and a new release.
static_assert(offsetof(Keypoint, x_px) == 0, "Keypoint.x_px moved");
static_assert(offsetof(Keypoint, y_px) == 4, "Keypoint.y_px moved");
static_assert(offsetof(Keypoint, confidence) == 8, "Keypoint.confidence moved");
static_assert(offsetof(Keypoint, label_id) == 12, "Keypoint.label_id moved");
static_assert(offsetof(Keypoint, flags) == 14, "Keypoint.flags moved");
static_assert(offsetof(Object, x_px) == 0, "Object.x_px moved");
static_assert(offsetof(Object, y_px) == 4, "Object.y_px moved");
static_assert(offsetof(Object, width_px) == 8, "Object.width_px moved");
static_assert(offsetof(Object, height_px) == 12, "Object.height_px moved");
static_assert(offsetof(Object, confidence) == 16, "Object.confidence moved");
static_assert(offsetof(Object, label_id) == 20, "Object.label_id moved");
static_assert(offsetof(Object, track_id) == 24, "Object.track_id moved");
static_assert(offsetof(Object, flags) == 28, "Object.flags moved");
static_assert(offsetof(Object, keypoint_count) == 32, "Object.keypoint_count moved");
static_assert(offsetof(Object, keypoints) == 36, "Object.keypoints moved");
static_assert(offsetof(Slot, magic) == 0, "Slot.magic moved");
static_assert(offsetof(Slot, schema_version) == 8, "Slot.schema_version moved");
static_assert(offsetof(Slot, slot_bytes) == 12, "Slot.slot_bytes moved");
static_assert(offsetof(Slot, sequence_id) == 16, "Slot.sequence_id moved");
static_assert(offsetof(Slot, payload_kind) == 24, "Slot.payload_kind moved");
static_assert(offsetof(Slot, flags) == 28, "Slot.flags moved");
static_assert(offsetof(Slot, state_stream_frame_id) == 32, "Slot.state_stream_frame_id moved");
static_assert(offsetof(Slot, source_stream_frame_id) == 40, "Slot.source_stream_frame_id moved");
static_assert(offsetof(Slot, camera_frame_id) == 48, "Slot.camera_frame_id moved");
static_assert(offsetof(Slot, recording_frame_id) == 56, "Slot.recording_frame_id moved");
static_assert(offsetof(Slot, camera_timestamp_ns) == 64, "Slot.camera_timestamp_ns moved");
static_assert(offsetof(Slot, timestamp_sys_ns) == 72, "Slot.timestamp_sys_ns moved");
static_assert(offsetof(Slot, orange_publish_timestamp_us_epoch) == 80, "Slot.orange_publish_timestamp_us_epoch moved");
static_assert(offsetof(Slot, orange_publish_timestamp_us_monotonic) == 88, "Slot.orange_publish_timestamp_us_monotonic moved");
static_assert(offsetof(Slot, recording_identity_token) == 96, "Slot.recording_identity_token moved");
static_assert(offsetof(Slot, camera_id) == 168, "Slot.camera_id moved");
static_assert(offsetof(Slot, camera_serial) == 172, "Slot.camera_serial moved");
static_assert(offsetof(Slot, source_width_px) == 204, "Slot.source_width_px moved");
static_assert(offsetof(Slot, source_height_px) == 208, "Slot.source_height_px moved");
static_assert(offsetof(Slot, detection_status) == 212, "Slot.detection_status moved");
static_assert(offsetof(Slot, pose_status) == 216, "Slot.pose_status moved");
static_assert(offsetof(Slot, detection_model_id_hash) == 224, "Slot.detection_model_id_hash moved");
static_assert(offsetof(Slot, pose_model_id_hash) == 232, "Slot.pose_model_id_hash moved");
static_assert(offsetof(Slot, pose_skeleton_id_hash) == 240, "Slot.pose_skeleton_id_hash moved");
static_assert(offsetof(Slot, object_count) == 248, "Slot.object_count moved");
static_assert(offsetof(Slot, objects) == 252, "Slot.objects moved");
static_assert(offsetof(Slot, source_detection_count) == 35324, "Slot.source_detection_count moved");
static_assert(offsetof(Slot, retained_detection_count) == 35328, "Slot.retained_detection_count moved");
static_assert(offsetof(Slot, transmitted_object_count) == 35332, "Slot.transmitted_object_count moved");
static_assert(offsetof(Slot, objects_truncated) == 35336, "Slot.objects_truncated moved");
static_assert(offsetof(Slot, object_order) == 35340, "Slot.object_order moved");
static_assert(offsetof(Slot, detection_reason) == 35344, "Slot.detection_reason moved");
static_assert(offsetof(Slot, producer_generation) == 35352, "Slot.producer_generation moved");
static_assert(offsetof(Slot, producer_instance_id) == 35360, "Slot.producer_instance_id moved");
static_assert(offsetof(SharedQueue, magic) == 0, "SharedQueue.magic moved");
static_assert(offsetof(SharedQueue, schema_version) == 8, "SharedQueue.schema_version moved");
static_assert(offsetof(SharedQueue, queue_bytes) == 12, "SharedQueue.queue_bytes moved");
static_assert(offsetof(SharedQueue, slot_bytes) == 16, "SharedQueue.slot_bytes moved");
static_assert(offsetof(SharedQueue, queue_size) == 20, "SharedQueue.queue_size moved");
static_assert(offsetof(SharedQueue, initialized) == 24, "SharedQueue.initialized moved");
static_assert(offsetof(SharedQueue, writer_sequence) == 32, "SharedQueue.writer_sequence moved");
static_assert(offsetof(SharedQueue, head) == 40, "SharedQueue.head moved");
static_assert(offsetof(SharedQueue, tail) == 48, "SharedQueue.tail moved");
static_assert(offsetof(SharedQueue, push_failures) == 56, "SharedQueue.push_failures moved");
static_assert(offsetof(SharedQueue, stale_suppressed) == 64, "SharedQueue.stale_suppressed moved");
static_assert(offsetof(SharedQueue, queue) == 72, "SharedQueue.queue moved");
static_assert(offsetof(SharedQueue, producer_generation) == 2263624, "SharedQueue.producer_generation moved");
static_assert(offsetof(SharedQueue, producer_instance_id) == 2263632, "SharedQueue.producer_instance_id moved");

inline uint64_t steady_time_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

inline uint64_t epoch_time_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// ABI4 uses the standard FNV-1a 64-bit offset basis. Keep this literal in the
// shared header so Orange and Citrus cannot silently derive different model
// identities for the same string.
inline uint64_t fnv1a64(const std::string& value)
{
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : value) {
        hash ^= static_cast<uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

inline std::string queue_name_for_camera_serial(const std::string& camera_serial)
{
    if (camera_serial.empty()) {
        throw std::invalid_argument("shaman v2 camera serial is empty");
    }
    if (camera_serial.size() >= sizeof(Slot::camera_serial)) {
        throw std::invalid_argument(
            "shaman v2 camera serial exceeds the fixed ABI field");
    }
    return "/shm_cam_" + camera_serial + "_v2";
}

inline bool slot_header_valid(const Slot& slot)
{
    return slot.magic == kMagic &&
           slot.schema_version == kSchemaVersion &&
           slot.slot_bytes == sizeof(Slot);
}

inline bool queue_header_valid(const SharedQueue& queue)
{
    return queue.magic == kMagic &&
           queue.schema_version == kSchemaVersion &&
           queue.queue_bytes == sizeof(SharedQueue) &&
           queue.slot_bytes == sizeof(Slot) &&
           queue.queue_size == kQueueSize;
}

inline bool detection_status_valid(uint32_t value)
{
    return value <= static_cast<uint32_t>(DetectionStatus::kFailed);
}

inline bool pose_status_valid(uint32_t value)
{
    return value <= static_cast<uint32_t>(PoseStatus::kFailed);
}

inline bool detection_reason_valid(uint32_t value)
{
    return value <= static_cast<uint32_t>(DetectionResultReason::kProcessingFailed);
}

// Validate the complete grouped-live payload before it crosses the SHM
// boundary. This is deliberately independent of slot_header_valid(): the
// writer stamps the header and generation immediately after this check.
inline bool slot_payload_valid(const Slot& slot)
{
    if (slot.payload_kind != static_cast<uint32_t>(PayloadKind::kLatestTrackingState) &&
        slot.payload_kind != static_cast<uint32_t>(PayloadKind::kStreamStatus)) {
        return false;
    }
    if (!detection_status_valid(slot.detection_status) ||
        !pose_status_valid(slot.pose_status) ||
        !detection_reason_valid(slot.detection_reason) ||
        slot.object_order != kObjectOrderUnorderedPayloadLocal ||
        slot.object_count > kMaxObjects ||
        slot.transmitted_object_count != slot.object_count ||
        slot.retained_detection_count < slot.transmitted_object_count ||
        slot.source_detection_count < slot.retained_detection_count ||
        slot.objects_truncated !=
            slot.retained_detection_count - slot.transmitted_object_count) {
        return false;
    }
    for (uint32_t index = 0; index < slot.object_count; ++index) {
        if (slot.objects[index].keypoint_count > kMaxKeypointsPerObject) {
            return false;
        }
    }

    const auto status = static_cast<DetectionStatus>(slot.detection_status);
    const auto reason = static_cast<DetectionResultReason>(slot.detection_reason);
    switch (status) {
        case DetectionStatus::kDisabled:
        case DetectionStatus::kNotScheduled:
        case DetectionStatus::kPending:
            return slot.source_detection_count == 0 &&
                   slot.retained_detection_count == 0 &&
                   slot.transmitted_object_count == 0 &&
                   slot.objects_truncated == 0 &&
                   reason == DetectionResultReason::kNone;
        case DetectionStatus::kDetections:
            return slot.retained_detection_count > 0 &&
                   slot.objects_truncated == 0 &&
                   reason == DetectionResultReason::kNone;
        case DetectionStatus::kZeroDetections:
            return slot.retained_detection_count == 0 &&
                   slot.transmitted_object_count == 0 &&
                   slot.objects_truncated == 0 &&
                   (reason == DetectionResultReason::kNoSourceDetections ||
                    reason == DetectionResultReason::kAllDetectionsRejectedByMask) &&
                   ((slot.source_detection_count == 0 &&
                     reason == DetectionResultReason::kNoSourceDetections) ||
                    (slot.source_detection_count > 0 &&
                     reason == DetectionResultReason::kAllDetectionsRejectedByMask));
        case DetectionStatus::kFailed:
            return reason != DetectionResultReason::kNone &&
                   ((reason == DetectionResultReason::kObjectsTruncated &&
                     slot.objects_truncated > 0) ||
                    (reason != DetectionResultReason::kObjectsTruncated &&
                     slot.objects_truncated == 0));
    }
    return false;
}

inline void copy_camera_serial(char (&dest)[32], const std::string& serial)
{
    std::memset(dest, 0, sizeof(dest));
    std::strncpy(dest, serial.c_str(), sizeof(dest) - 1);
}

inline void copy_recording_identity_token(
    char (&dest)[72],
    const std::string& token)
{
    std::memset(dest, 0, sizeof(dest));
    if (!token.empty()) {
        std::strncpy(dest, token.c_str(), sizeof(dest) - 1);
    }
}

// The index lock is intentionally outside the shared-memory object so its
// synchronization cannot change the ABI4 layout. Keep this helper public and
// byte-for-byte identical in Citrus: queue-name sanitization is only a human
// readable prefix; the FNV suffix prevents names such as '-' and '_' from
// aliasing the same lock file.
inline std::string index_lock_path(const std::string& queue_name)
{
    std::ostringstream suffix;
    suffix << std::hex << std::nouppercase << std::setfill('0')
           << std::setw(16) << fnv1a64(queue_name);
    std::string path = "/tmp/orange_shaman_v2_index";
    path.reserve(path.size() + queue_name.size() + suffix.str().size() + 2);
    for (const unsigned char byte : queue_name) {
        const bool ascii_alnum =
            (byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9');
        path.push_back(ascii_alnum ? static_cast<char>(byte) : '_');
    }
    path.push_back('_');
    path += suffix.str();
    path += ".lock";
    return path;
}

} // namespace shaman_v2
