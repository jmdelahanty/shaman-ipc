// Layout and protocol-helper checks. The static_asserts in the header already fail the build on
// any layout change; this test pins the helpers both sides must compute identically.
#include "shaman_ipc/shaman_v2_wire.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
}  // namespace

int main()
{
    using namespace shaman_v2;
    check(kSchemaVersion == 4, "schema version is 4");
    check(kMagic == 0x4f524e4753484d32ULL, "magic is ORNGSHM2");
    check(sizeof(Slot) == 35368 && sizeof(SharedQueue) == 2263640, "slot and queue sizes");
    check(queue_name_for_camera_serial("2010093") == "/shm_cam_2010093_v2", "queue name");
    check(fnv1a64("") == 14695981039346656037ULL, "fnv1a64 offset basis");
    check(fnv1a64("a") == 0xaf63dc4c8601ec8cULL, "fnv1a64 of 'a'");
    check(index_lock_path("/shm_cam_2010093_v2") ==
              "/tmp/orange_shaman_v2_index_shm_cam_2010093_v2_" +
                  [] {
                      std::ostringstream s;
                      s << std::hex << std::setfill('0') << std::setw(16)
                        << fnv1a64("/shm_cam_2010093_v2");
                      return s.str();
                  }() + ".lock",
          "index lock path");
    Slot slot;
    check(slot_header_valid(slot), "default slot header valid");
    slot.schema_version = 3;
    check(!slot_header_valid(slot), "older schema version refused");
    if (failures == 0) {
        std::printf("shaman_ipc layout test passed\n");
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
