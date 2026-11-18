#include "storage.hpp"
#include <switch.h>
namespace re5_overlay_storage {
namespace { bool mounted = false; }
bool mount() {
    if (mounted) return true;
    FsFileSystem fs{};
    if (R_FAILED(fsOpenSdCardFileSystem(&fs))) return false;
    // MountDevice takes ownership of the handle, including on failure.
    mounted = fsdevMountDevice("r5ovl", fs) >= 0;
    return mounted;
}
bool ready() { return mounted; }
void close() {
    if (mounted) fsdevUnmountDevice("r5ovl");
    mounted = false;
}
}
