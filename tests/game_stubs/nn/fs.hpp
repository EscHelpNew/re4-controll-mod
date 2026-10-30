#pragma once
#include <cstddef>
#include <cstring>
#include <string>
namespace nn::fs {
struct FileHandle { unsigned value=0; };
inline Result MountSdCardForDebug(const char*) {
    ++game_test::mounts;
    if(game_test::mount_failures){--game_test::mount_failures;return 7;}
    return game_test::mount_result;
}
inline const auto& file(FileHandle h){return h.value==1?game_test::settings_primary:game_test::settings_backup;}
inline Result OpenFile(FileHandle* h,const char* path,int) {
    ++game_test::fs_opens;h->value=std::string(path).find(".previous")==std::string::npos?1:2;
    return file(*h).empty()?9:0;
}
inline Result GetFileSize(long* size,FileHandle h){*size=long(file(h).size());return 0;}
inline Result ReadFile(FileHandle h,int,void* out,std::size_t size){++game_test::fs_reads;std::memcpy(out,file(h).data(),size);return 0;}
inline void CloseFile(FileHandle){++game_test::fs_closes;}
}
