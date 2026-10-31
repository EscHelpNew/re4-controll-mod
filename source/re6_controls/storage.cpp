#include "storage.hpp"
#include <switch.h>
namespace re6_overlay_storage {
namespace { bool mounted = false; }
bool mount() {
    if (mounted) return true;
    FsFileSystem fs{};
    if (R_FAILED(fsOpenSdCardFileSystem(&fs))) return false;
    // MountDevice takes ownership of the handle, including on failure.
    mounted = fsdevMountDevice("r6ovl", fs) >= 0;
    return mounted;
}
bool ready() { return mounted; }
void close() {
    if (mounted) fsdevUnmountDevice("r6ovl");
    mounted = false;
}
}
