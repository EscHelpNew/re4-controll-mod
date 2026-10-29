#pragma once
#include <cstddef>
namespace nn::fs {
struct FileHandle { unsigned value=0; };
inline Result MountSdCardForDebug(const char*) { ++game_test::mounts; return 1; }
inline Result OpenFile(FileHandle*, const char*, int) { ++game_test::fs_opens; return 1; }
inline Result GetFileSize(long*, FileHandle) { return 1; }
inline Result ReadFile(FileHandle, int, void*, std::size_t) { return 1; }
inline void CloseFile(FileHandle) {}
}
