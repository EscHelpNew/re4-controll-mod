#include "re4/home_launcher.hpp"
#include <cstring>
namespace re4 {
namespace {
std::uint32_t le32(const std::uint8_t* p){return p[0]|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
std::uint64_t le64(const std::uint8_t* p){return le32(p)|std::uint64_t(le32(p+4))<<32;}
}
bool parse_home_bundle(const std::uint8_t* data,std::size_t size,HomeVerify verify,HomeBundle& out){
    constexpr unsigned kinds[]={0,1,3};
    constexpr std::uint8_t header[]={16,0,3,0,0,0,0,0};
    if(!data || !verify || size<320 || size>16*1024*1024 || std::memcmp(data,"R4HOME01",8)
        || le64(data+8)!=kOwnLauncherId || le32(data+20)!=96 || le32(data+24)!=3 || le32(data+28)
        || std::memcmp(data+32,header,8))return false;
    HomeBundle bundle;bundle.title=kOwnLauncherId;bundle.version=le32(data+16);
    bundle.database.assign(data+32,data+128);
    std::size_t offset=320;
    for(unsigned i=0;i<3;++i){
        const auto* record=data+128+i*64;
        const auto bytes=le32(record+48);
        if(!bytes || bytes>4*1024*1024 || bytes>size-offset || record[52]!=kinds[i]
            || std::memcmp(record,record+16,16))return false;
        for(unsigned j=53;j<64;++j)if(record[j])return false;
        const auto* info=bundle.database.data()+24+i*24;
        if(std::memcmp(info,record,16) || le32(info+16)!=bytes || info[20] || info[21]
            || info[22]!=kinds[i] || info[23] || !verify(data+offset,bytes,record+16))return false;
        auto& content=bundle.contents[i];
        std::memcpy(content.id.data(),record,16);std::memcpy(content.hash.data(),record+16,32);
        content.type=std::uint8_t(kinds[i]);content.data.assign(data+offset,data+offset+bytes);
        offset+=bytes;
    }
    if(offset!=size)return false;
    out=std::move(bundle);return true;
}
}
