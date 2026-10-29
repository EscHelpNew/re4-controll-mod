#include "re4/update.hpp"
#include "reload_core.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <sys/stat.h>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace re4 {
namespace {
bool number(std::string_view text,std::uint32_t limit,std::uint32_t& out){
    if(text.empty() || (text.size()>1 && text[0]=='0'))return false;
    std::uint32_t n=0;
    for(char c:text){if(c<'0'||c>'9'||n>(limit-unsigned(c-'0'))/10)return false;n=n*10+unsigned(c-'0');}
    out=n;return true;
}
bool version(std::string_view text,std::array<std::uint32_t,3>& out){
    for(unsigned i=0;i<3;++i){
        const auto dot=text.find('.');
        if((i<2 && dot==text.npos) || (i==2 && dot!=text.npos))return false;
        const auto part=i<2?text.substr(0,dot):text;
        if(!number(part,65535,out[i]))return false;
        if(i<2)text.remove_prefix(dot+1);
    }
    return true;
}
int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
bool exists(const std::string& path){struct stat st{};return !stat(path.c_str(),&st);}
bool copy(const std::string& source,const std::string& destination){
    FILE* in=std::fopen(source.c_str(),"rb");if(!in)return false;
    FILE* out=std::fopen(destination.c_str(),"wb");if(!out){std::fclose(in);return false;}
    unsigned char bytes[16384];bool ok=true;std::size_t got;
    while((got=std::fread(bytes,1,sizeof(bytes),in))>0)if(std::fwrite(bytes,1,got,out)!=got){ok=false;break;}
    ok=ok && !std::ferror(in);
    if(std::fflush(out))ok=false;
#ifdef _WIN32
    if(_commit(_fileno(out)))ok=false;
#else
    if(fsync(fileno(out)))ok=false;
#endif
    if(std::fclose(in))ok=false;
    if(std::fclose(out))ok=false;
    return ok;
}
bool own_nro_version(const std::string& path,std::string& out){
    if(!reload_bridge_valid(path.c_str()))return false;
    struct stat st{};if(stat(path.c_str(),&st) || st.st_size<0)return false;
    FILE* file=std::fopen(path.c_str(),"rb");if(!file)return false;
    unsigned char header[128]{},asset[40]{};std::vector<unsigned char> nacp(0x4000);
    bool ok=std::fread(header,1,sizeof(header),file)==sizeof(header);
    std::uint32_t image=0;std::memcpy(&image,header+0x18,4);
    ok=ok && image<=std::uint64_t(st.st_size) && std::uint64_t(st.st_size)-image>=40
        && !std::fseek(file,image,SEEK_SET) && std::fread(asset,1,sizeof(asset),file)==sizeof(asset)
        && !std::memcmp(asset,"ASET",4);
    std::uint64_t offset=0,bytes=0;
    if(ok){std::memcpy(&offset,asset+24,8);std::memcpy(&bytes,asset+32,8);}
    ok=ok && offset<=std::uint64_t(st.st_size)-image && bytes==nacp.size()
        && bytes<=std::uint64_t(st.st_size)-image-offset
        && !std::fseek(file,long(image+offset),SEEK_SET) && std::fread(nacp.data(),1,nacp.size(),file)==nacp.size();
    if(std::fclose(file))ok=false;
    std::uint64_t title=0;if(ok)std::memcpy(&title,nacp.data()+0x3038,8);
    if(!ok || title!=0x0100E4C04E540000ull || std::memcmp(nacp.data(),"RE4 Controls\0",13)
        || std::memcmp(nacp.data()+0x200,"re4-controls\0",13))return false;
    unsigned length=0;while(length<16 && nacp[0x3060+length])++length;
    if(length==16)return false;
    std::string value(reinterpret_cast<const char*>(nacp.data()+0x3060),length);
    std::array<std::uint32_t,3> parts{};if(!version(value,parts))return false;
    out=value;return true;
}
}
bool newer_version(const std::string& candidate,const std::string& installed){
    std::array<std::uint32_t,3> a{},b{};
    return version(candidate,a) && version(installed,b) && a>b;
}
bool parse_release_manifest(const std::string& text,ReleaseInfo& out){
    if(text.size()>512)return false;
    constexpr const char* prefixes[]={"RE4-CONTROLS-UPDATE 1","version=","bytes=","sha256="};
    std::string values[4];std::size_t offset=0;
    for(unsigned i=0;i<4;++i){
        const auto end=text.find('\n',offset);if(end==text.npos)return false;
        const auto line=text.substr(offset,end-offset);const auto length=std::strlen(prefixes[i]);
        if(line.compare(0,length,prefixes[i]))return false;
        values[i]=line.substr(length);offset=end+1;
    }
    if(offset!=text.size() || !values[0].empty())return false;
    ReleaseInfo info;std::array<std::uint32_t,3> parts{};
    if(!version(values[1],parts) || !number(values[2],kUpdateMaxBytes,info.bytes) || info.bytes<128 || values[3].size()!=64)return false;
    info.version=values[1];
    for(unsigned i=0;i<32;++i){const int a=hex(values[3][i*2]),b=hex(values[3][i*2+1]);if(a<0||b<0)return false;info.sha256[i]=unsigned(a*16+b);}
    out=info;return true;
}
int UpdateFileOps::move(const char* from,const char* to){return std::rename(from,to);}
bool bootstrap_self_images(const std::string& source,const std::string& root,UpdateHash hash,UpdateFileOps& ops,std::string& detail){
    std::string bridge,game,current;
    const bool has_bridge=own_nro_version(root+"/bridge.nro",bridge);
    const bool has_game=own_nro_version(root+"/game.nro",game);
    std::string chosen=source;
    bool available=own_nro_version(source,current);
    if(has_bridge && (!available || newer_version(bridge,current))){
        chosen=root+"/bridge.nro";current=bridge;available=true;
    }
    if(has_game && (!available || newer_version(game,current))){
        chosen=root+"/game.nro";current=game;available=true;
    }
    if(!available){
        detail="Launch the downloaded NRO from the SD card to create its HOME shortcut";return false;
    }
    if(has_bridge && has_game && !newer_version(current,bridge) && !newer_version(current,game)){
        detail="App images are ready";return true;
    }
    struct stat st{};ReleaseInfo info;info.version=current;
    if(!hash || stat(chosen.c_str(),&st) || st.st_size<128 || std::uint64_t(st.st_size)>kUpdateMaxBytes
        || !hash(chosen.c_str(),info.sha256.data())){detail="Could not verify this app image";return false;}
    info.bytes=std::uint32_t(st.st_size);
    const auto download=root+"/update.nro.download";
    if(chosen!=download && !copy(chosen,download)){detail="Could not copy this app to its HOME directory";return false;}
    return publish_update(root,info,hash,ops,detail);
}
bool publish_update(const std::string& root,const ReleaseInfo& info,UpdateHash hash,UpdateFileOps& ops,std::string& detail){
    const auto source=root+"/update.nro.download";
    struct stat st{};unsigned char digest[32],header[128];
    if(!hash || info.bytes<128 || info.bytes>kUpdateMaxBytes || stat(source.c_str(),&st) || st.st_size<0 || std::uint64_t(st.st_size)!=info.bytes
        || !hash(source.c_str(),digest) || std::memcmp(digest,info.sha256.data(),32)){
        detail="Update size or SHA256 verification failed";return false;
    }
    FILE* file=std::fopen(source.c_str(),"rb");if(!file){detail="Could not read update";return false;}
    const auto got=std::fread(header,1,sizeof(header),file);const bool closed=std::fclose(file)==0;
    if(!closed || !reload_nro_valid(header,got,info.bytes) || !reload_bridge_valid(source.c_str())){
        detail="Update is not a compatible RE4 Controls NRO";return false;
    }
    std::string embedded_version;
    if(!own_nro_version(source,embedded_version) || embedded_version!=info.version){
        detail="Update application identity or version does not match the release";return false;
    }
    const std::string final[]={root+"/game.nro",root+"/bridge.nro"};
    bool moved[2]{},published[2]{};
    for(unsigned i=0;i<2;++i)if(!copy(source,final[i]+".ota")){
        detail="Could not stage update; current app kept";return false;
    }
    for(unsigned i=0;i<2;++i){
        const auto backup=final[i]+".ota-backup";
        if(!exists(final[i]) && exists(backup) && ops.move(backup.c_str(),final[i].c_str())){
            detail="Could not restore an interrupted update; recovery copy kept";return false;
        }
        if(exists(backup) && std::remove(backup.c_str())){detail="Previous update backup is unavailable";return false;}
    }
    for(unsigned i=0;i<2;++i){
        const auto backup=final[i]+".ota-backup";
        if(exists(final[i])){
            if(ops.move(final[i].c_str(),backup.c_str()))goto rollback;
            moved[i]=true;
        }
        if(ops.move((final[i]+".ota").c_str(),final[i].c_str()))goto rollback;
        published[i]=true;
    }
    for(unsigned i=0;i<2;++i)if(moved[i]){
        const auto previous=final[i]+".previous";
        std::remove(previous.c_str());
        ops.move((final[i]+".ota-backup").c_str(),previous.c_str());
    }
    std::remove(source.c_str());detail="Update installed. Reopen RE4 Controls.";return true;
rollback:
    bool restored=true;
    for(unsigned i=0;i<2;++i){
        if(published[i])std::remove(final[i].c_str());
        if(moved[i] && ops.move((final[i]+".ota-backup").c_str(),final[i].c_str()))restored=false;
    }
    detail=restored?"Update failed; previous app restored":"Update failed; recovery copies kept (.ota-backup)";
    return false;
}
}
