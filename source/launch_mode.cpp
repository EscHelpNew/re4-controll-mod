#include "re4/launch_mode.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <cerrno>
#ifdef _WIN32
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace re4 {
namespace {
enum class FileKind {Missing,Regular,Other};
FileKind file_kind(const char* path){
#ifdef _WIN32
    const auto attributes=GetFileAttributesA(path);
    if(attributes==INVALID_FILE_ATTRIBUTES){
        const auto error=GetLastError();
        return error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND?FileKind::Missing:FileKind::Other;
    }
    return attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)?FileKind::Other:FileKind::Regular;
#else
    struct stat info{};
    if(stat(path,&info))return errno==ENOENT?FileKind::Missing:FileKind::Other;
    return S_ISREG(info.st_mode)?FileKind::Regular:FileKind::Other;
#endif
}
}
bool load_launch_mode(const char* path,LaunchMode& out){
    out=LaunchMode::Normal;
    if(!path||!*path)return false;
    FILE* file=std::fopen(path,"rb");if(!file)return false;
    std::uint8_t bytes[9]{};const auto count=std::fread(bytes,1,sizeof(bytes),file);
    const bool read=!std::ferror(file);const bool closed=std::fclose(file)==0;
    if(!read||!closed||count!=8||std::memcmp(bytes,"R4LM",4)||bytes[4]!=1||bytes[5]>1
        ||bytes[6]!=std::uint8_t(~bytes[5])||bytes[7])return false;
    out=static_cast<LaunchMode>(bytes[5]);return true;
}
bool save_launch_mode(const char* path,LaunchMode mode){
    if(!path||!*path||std::strlen(path)>700||!valid_launch_mode(mode))return false;
    const auto original=file_kind(path);const bool had_original=original==FileKind::Regular;
    if(original==FileKind::Other)return false;
    const std::string incoming=std::string(path)+".incoming",previous=std::string(path)+".previous";
    const auto value=std::uint8_t(mode);
    const std::uint8_t bytes[]={'R','4','L','M',1,value,std::uint8_t(~value),0};
    FILE* file=std::fopen(incoming.c_str(),"wb");if(!file)return false;
    bool written=std::fwrite(bytes,1,sizeof(bytes),file)==sizeof(bytes)&&std::fflush(file)==0;
#ifdef _WIN32
    if(written)written=_commit(_fileno(file))==0;
#else
    if(written)written=fsync(fileno(file))==0;
#endif
    const bool closed=std::fclose(file)==0;
    if(!written||!closed){std::remove(incoming.c_str());return false;}
    if(had_original){
        const auto backup=file_kind(previous.c_str());
        if(backup==FileKind::Other||(backup==FileKind::Regular&&std::remove(previous.c_str())))return false;
        if(std::rename(path,previous.c_str()))return false;
    }
    if(!std::rename(incoming.c_str(),path))return true;
    if(had_original)std::rename(previous.c_str(),path);
    return false;
}
}
