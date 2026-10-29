#include "storage.hpp"
#include <switch.h>
namespace overlay_storage {
namespace { bool mounted = false; }
bool mount() {
    if (mounted) return true;
    FsFileSystem fs{};
    if (R_FAILED(fsOpenSdCardFileSystem(&fs))) return false;
    // fsdevMountDevice takes ownership even on failure. Keep our alias separate
    // from temporary sdmc mounts that other overlays/library helpers may use.
    mounted = fsdevMountDevice("r4ovl", fs) >= 0;
    return mounted;
}
bool ready() { return mounted; }
void close() {
    if (mounted) fsdevUnmountDevice("r4ovl");
    mounted = false;
}
}
