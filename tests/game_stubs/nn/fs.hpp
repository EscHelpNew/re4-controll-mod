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
inline const auto& file(FileHandle h){return h.value>=5?game_test::money_files[h.value-5]:h.value==4?game_test::extended_upgrade_file:h.value==3?game_test::resource_file:h.value==1?game_test::settings_primary:game_test::settings_backup;}
inline Result OpenFile(FileHandle* h,const char* path,int mode) {
    ++game_test::fs_opens;
    const std::string name(path);
    if(name.find("money-bonus")!=std::string::npos){
        h->value=name.find("money-bonus-a")!=std::string::npos?5:name.find("money-bonus-b")!=std::string::npos?6:7;
        if(game_test::money_open_result)return game_test::money_open_result;
        if(h->value<7&&game_test::money_legacy_open_result)return game_test::money_legacy_open_result;
        if((mode&2)&&game_test::money_open_write_result)return game_test::money_open_write_result;
        return game_test::money_exists[h->value-5]?0:0x202;
    }
    h->value=name.find("extended-upgrade")!=std::string::npos?4:name==game_test::resource_path?3:name.find(".previous")==std::string::npos?1:2;
    if(h->value==4){
        const auto failure=(mode&2)?game_test::extended_upgrade_open_write_result:game_test::extended_upgrade_open_read_result;
        if(failure)return failure;
        return game_test::extended_upgrade_file_exists||!file(*h).empty()?0:0x202;
    }
    return file(*h).empty()?9:0;
}
inline Result GetFileSize(long* size,FileHandle h){
    if(h.value>=5&&game_test::money_size_result)return game_test::money_size_result;
    if(h.value==4&&game_test::extended_upgrade_size_result)return game_test::extended_upgrade_size_result;
    *size=long(file(h).size());return 0;
}
inline Result ReadFile(FileHandle h,std::size_t offset,void* out,std::size_t size){
    ++game_test::fs_reads;
    if(h.value>=5&&game_test::money_read_result)return game_test::money_read_result;
    if(h.value==4&&game_test::extended_upgrade_read_result)return game_test::extended_upgrade_read_result;
    if(offset>file(h).size()||size>file(h).size()-offset)return 9;
    if(h.value==3)game_test::resource_max_read=std::max(game_test::resource_max_read,size);
    std::memcpy(out,file(h).data()+offset,size);return 0;
}
inline void CloseFile(FileHandle){++game_test::fs_closes;}
enum WriteOptionFlag {WriteOptionFlag_Flush=1};
struct WriteOption {int flags=0;static WriteOption CreateOption(int v){return {v};}};
inline Result CreateFile(const char* path,long size){
    const std::string name(path);
    if(name.find("money-bonus")!=std::string::npos){
        const unsigned index=name.find("money-bonus-a")!=std::string::npos?0:name.find("money-bonus-b")!=std::string::npos?1:2;
        if(game_test::money_create_result)return game_test::money_create_result;
        if(game_test::money_create_race){game_test::money_exists[index]=true;game_test::money_create_race=false;}
        if(game_test::money_exists[index])return 0x402;
        game_test::money_exists[index]=true;game_test::money_files[index].resize(std::size_t(size));return 0;
    }
    if(std::string(path).find("extended-upgrade")==std::string::npos)return 0;
    if(game_test::extended_upgrade_create_result)return game_test::extended_upgrade_create_result;
    if(game_test::extended_upgrade_file_exists||!game_test::extended_upgrade_file.empty())return 9;
    game_test::extended_upgrade_file_exists=true;game_test::extended_upgrade_file.resize(std::size_t(size));return 0;
}
inline Result WriteFile(FileHandle h,long offset,const void* data,unsigned long size,const WriteOption& option){
    if(h.value>=5){
        game_test::money_write_flags=option.flags;
        ++game_test::money_writes;if(game_test::money_write_result)return game_test::money_write_result;
        auto& target=game_test::money_files[h.value-5];
        if(offset<0||std::size_t(offset)>target.size()||size>target.size()-std::size_t(offset))return 9;
        std::memcpy(target.data()+offset,data,size);return 0;
    }
    if(h.value!=4)return 9; // Only the sidecar is ever written by the game module.
    ++game_test::extended_upgrade_writes;
    if(game_test::extended_upgrade_write_result)return game_test::extended_upgrade_write_result;
    auto& target=game_test::extended_upgrade_file;
    if(offset<0||std::size_t(offset)>target.size())return 9;
    if(target.size()<std::size_t(offset)+size)target.resize(std::size_t(offset)+size);
    std::memcpy(target.data()+offset,data,size);return 0;
}
}
