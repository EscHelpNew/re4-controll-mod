#pragma once
using Result = unsigned;
struct FsFileSystem { unsigned handle = 0; };
#define R_FAILED(value) ((value) != 0)
Result fsOpenSdCardFileSystem(FsFileSystem*);
int fsdevMountDevice(const char*, FsFileSystem);
int fsdevUnmountDevice(const char*);
