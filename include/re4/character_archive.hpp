#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace re4 {
struct CharacterBytes {const std::uint8_t* data=nullptr;std::size_t size=0;};
struct CharacterArchive {CharacterBytes dat{};unsigned count=0;};
constexpr std::size_t character_archive_limit=32*1024*1024;
inline bool character_subspan(CharacterBytes input,std::uint64_t offset,std::uint64_t size,
                              CharacterBytes& out){
    if(!input.data||offset>input.size||size>input.size-offset)return false;
    out={input.data+std::size_t(offset),std::size_t(size)};return true;
}
template<class T>inline bool character_read(CharacterBytes input,std::size_t offset,T& out){
    CharacterBytes bytes;
    if(!character_subspan(input,offset,sizeof(T),bytes))return false;
    std::memcpy(&out,bytes.data,sizeof(T));return true;
}
// Only the raw uncompressed UDAS variant is accepted. No typed module/resource
// loader runs, and no in-place pointer relocation happens during validation.
inline bool character_archive(CharacterBytes input,CharacterArchive& out){
    if(!input.data||input.size<0x400||input.size>character_archive_limit)return false;
    for(unsigned i=0;i<8;++i){
        std::uint32_t magic=0;
        if(!character_read(input,i*4,magic)||magic!=0x20BEB6CA)return false;
    }
    CharacterArchive value;bool ended=false,found=false;
    for(std::size_t p=0x20;p<0x400;p+=0x20){
        std::uint32_t kind=0,size=0,target=0,offset=0;
        character_read(input,p,kind);
        if(kind==0xFFFFFFFF){ended=true;break;}
        character_read(input,p+4,size);character_read(input,p+8,target);character_read(input,p+12,offset);
        CharacterBytes span;
        if((kind!=0&&kind!=4)||target||(size&&offset<0x400)
            ||!character_subspan(input,offset,size,span))return false;
        if(kind==0){
            if(found||size<16||offset<0x400)return false;
            value.dat=span;found=true;
        }
    }
    if(!ended||!found)return false;
    std::uint32_t count=0;
    if(!character_read(value.dat,0,count)||!count||count>4096)return false;
    const std::size_t directory=16+std::size_t(count)*8;
    if(directory>value.dat.size)return false;
    std::uint32_t previous=std::uint32_t(directory);
    for(unsigned i=0;i<count;++i){
        std::uint32_t offset=0;
        character_read(value.dat,16+i*4,offset);
        if(offset<previous||offset>=value.dat.size)return false;
        previous=offset;
    }
    value.count=count;out=value;return true;
}
inline bool character_archive_entry(const CharacterArchive& archive,unsigned index,
                                     const char* type,CharacterBytes& out){
    if(!type||index>=archive.count||!archive.count||archive.count>4096
        ||archive.dat.size>character_archive_limit)return false;
    const std::size_t directory=16+std::size_t(archive.count)*8;
    if(directory>archive.dat.size)return false;
    CharacterBytes tag;
    if(!character_subspan(archive.dat,16+std::size_t(archive.count)*4+index*4,4,tag)
        ||std::memcmp(tag.data,type,3)||tag.data[3])return false;
    std::uint32_t begin=0,end=std::uint32_t(archive.dat.size);
    if(!character_read(archive.dat,16+index*4,begin))return false;
    if(index+1<archive.count&&!character_read(archive.dat,16+(index+1)*4,end))return false;
    if(begin<directory||end<=begin)return false;
    return character_subspan(archive.dat,begin,std::uint64_t(end)-begin,out);
}
}
