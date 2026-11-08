#include "re4/character_archive.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
namespace {
using namespace re4;
template<class T>void put(std::vector<std::uint8_t>& data,std::size_t at,T value){std::memcpy(data.data()+at,&value,sizeof(value));}
std::vector<std::uint8_t> archive(){
    std::vector<std::uint8_t> data(0x600);
    for(unsigned i=0;i<8;++i)put<std::uint32_t>(data,i*4,0x20BEB6CA);
    put<std::uint32_t>(data,0x20,0);put<std::uint32_t>(data,0x24,0x180);put<std::uint32_t>(data,0x2C,0x420);
    put<std::uint32_t>(data,0x40,4);put<std::uint32_t>(data,0x4C,0x5C0);put<std::uint32_t>(data,0x60,0xFFFFFFFF);
    put<std::uint32_t>(data,0x420,2);
    put<std::uint32_t>(data,0x430,0x20);put<std::uint32_t>(data,0x434,0xA0);
    std::memcpy(data.data()+0x438,"BIN\0TPL\0",8);
    return data;
}
}
int main(){
    using namespace re4;
    auto data=archive();CharacterArchive parsed;
    assert(character_archive({data.data(),data.size()},parsed));
    assert(parsed.count==2&&parsed.dat.size==0x180);
    CharacterBytes bin,tpl;
    assert(character_archive_entry(parsed,0,"BIN",bin)&&bin.data==data.data()+0x440&&bin.size==0x80);
    assert(character_archive_entry(parsed,1,"TPL",tpl)&&tpl.size==0xE0);
    const auto old=bin;
    assert(!character_archive_entry(parsed,2,"BIN",bin)&&bin.data==old.data&&bin.size==old.size);
    assert(!character_archive_entry(parsed,0,"TPL",bin));
    assert(!character_archive({nullptr,data.size()},parsed));
    for(std::size_t length=0;length<0x5A0;++length)
        assert(!character_archive({data.data(),length},parsed));
    auto reject=[&](std::size_t offset,std::uint32_t value){
        auto bad=data;put(bad,offset,value);CharacterArchive sentinel=parsed;
        assert(!character_archive({bad.data(),bad.size()},sentinel));
        assert(sentinel.dat.data==parsed.dat.data&&sentinel.count==parsed.count);
    };
    reject(0,0);reject(0x28,1);reject(0x2C,0xFFFFFFF0);reject(0x24,0xFFFF);
    reject(0x40,3);reject(0x4C,0x700);reject(0x420,4097);reject(0x430,0x1F);
    reject(0x434,0x10);reject(0x434,0x181);
    // A second root data record is ambiguous, even when both spans are valid.
    reject(0x40,0);
    // Unbounded record tables must terminate within the fixed header.
    auto bad=data;put<std::uint32_t>(bad,0x60,4);
    for(std::size_t p=0x80;p<0x400;p+=0x20)put<std::uint32_t>(bad,p,4);
    assert(!character_archive({bad.data(),bad.size()},parsed));
    std::puts("character archive: bounded raw UDAS/DAT, typed spans, truncation and ambiguity rejection PASS");
}
