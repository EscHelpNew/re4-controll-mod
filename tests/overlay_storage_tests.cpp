#include "storage.hpp"
#include <switch.h>
#include <cassert>
#include <cstdio>
#include <cstring>
unsigned opens = 0, mounts = 0, unmounts = 0;
Result open_result = 1;
int mount_result = -1;
Result fsOpenSdCardFileSystem(FsFileSystem* out) { ++opens; out->handle = 42; return open_result; }
int fsdevMountDevice(const char* name, FsFileSystem fs) {
    assert(!std::strcmp(name, "r4ovl") && fs.handle == 42); ++mounts; return mount_result;
}
int fsdevUnmountDevice(const char* name) { assert(!std::strcmp(name, "r4ovl")); ++unmounts; return 0; }
int main() {
    assert(!overlay_storage::ready());
    assert(!overlay_storage::mount() && mounts == 0); // No device registration after open failure.
    open_result = 0;
    assert(!overlay_storage::mount() && !overlay_storage::ready());
    overlay_storage::close(); assert(unmounts == 0); // MountDevice owns/cleans a failed FS handle.
    mount_result = 3;
    assert(overlay_storage::mount() && overlay_storage::ready());
    const auto calls = opens;
    assert(overlay_storage::mount() && opens == calls); // No duplicate device registration.
    overlay_storage::close(); overlay_storage::close();
    assert(!overlay_storage::ready() && unmounts == 1);
    std::puts("overlay SD lifecycle: failed open/mount, ownership and idempotent close PASS");
}
