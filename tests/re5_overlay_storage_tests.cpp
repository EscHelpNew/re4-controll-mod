#include "storage.hpp"
#include <switch.h>
#include <cassert>
#include <cstdio>
#include <cstring>
namespace {
unsigned opens = 0, mounts = 0, unmounts = 0;
Result open_result = 1;
int mount_result = -1;
}
Result fsOpenSdCardFileSystem(FsFileSystem* out) { ++opens; out->handle = 42; return open_result; }
int fsdevMountDevice(const char* name, FsFileSystem fs) {
    assert(!std::strcmp(name, "r5ovl") && fs.handle == 42); ++mounts; return mount_result;
}
int fsdevUnmountDevice(const char* name) { assert(!std::strcmp(name, "r5ovl")); ++unmounts; return 0; }
int main() {
    assert(!re5_overlay_storage::ready());
    assert(!re5_overlay_storage::mount() && opens == 1 && mounts == 0);
    open_result = 0;
    assert(!re5_overlay_storage::mount() && mounts == 1 && !re5_overlay_storage::ready());
    re5_overlay_storage::close(); assert(unmounts == 0);
    mount_result = 3;
    assert(re5_overlay_storage::mount() && re5_overlay_storage::ready());
    const auto calls = opens;
    assert(re5_overlay_storage::mount() && opens == calls);
    re5_overlay_storage::close(); re5_overlay_storage::close();
    assert(!re5_overlay_storage::ready() && unmounts == 1);
    std::puts("RE5 overlay SD: isolated alias, failed open/mount, ownership and idempotent close PASS");
}
