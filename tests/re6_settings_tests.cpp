#include "re6/settings.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
namespace {
using Bytes = std::array<std::uint8_t, re6::kSettingsBytes>;
void put32(std::uint8_t* p, std::uint32_t v) { for (unsigned i=0;i<4;++i) p[i]=std::uint8_t(v>>(8*i)); }
void seal(Bytes& b) { std::uint32_t h=2166136261u; for(unsigned i=0;i<60;++i){h^=b[i];h*=16777619u;}put32(b.data()+60,h); }
bool equal(const re6::Settings& a,const re6::Settings& b){return a.language==b.language && a.texture_filtering==b.texture_filtering && a.anti_aliasing==b.anti_aliasing;}
void rejects(const Bytes& b,std::size_t size=64){
    re6::Settings out;out.language=re6::Language::Russian;out.texture_filtering=re6::TextureFilter::Af8x;const auto before=out;
    assert(!re6::decode_settings(b.data(),size,out) && equal(out,before));
}
void wire(){
    Bytes good{};assert(re6::encode_settings({},good.data(),good.size()));
    assert(!std::memcmp(good.data(),"R6CT",4) && good[4]==1 && good[8]==0 && good[12]==0);
    for(unsigned i=13;i<60;++i)assert(good[i]==0);
    re6::Settings value,out;
    for(unsigned l=0;l<2;++l)for(unsigned f=0;f<5;++f)for(unsigned aa=0;aa<5;++aa){
        value.language=static_cast<re6::Language>(l);value.texture_filtering=static_cast<re6::TextureFilter>(f);
        value.anti_aliasing=static_cast<re6::AntiAliasing>(aa);
        Bytes b{};assert(re6::encode_settings(value,b.data(),64) && b[8]==l && b[12]==f && b[13]==aa);
        assert(re6::decode_settings(b.data(),64,out) && equal(value,out));
    }
    auto b=good;b[0]='4';seal(b);rejects(b);
    for(auto v:{0u,2u,0xffffffffu}){b=good;put32(b.data()+4,v);seal(b);rejects(b);}
    for(unsigned bit=1;bit<32;++bit){b=good;put32(b.data()+8,1u<<bit);seal(b);rejects(b);}
    for(unsigned i=14;i<60;++i){b=good;b[i]=1;seal(b);rejects(b);}
    b=good;b[12]=5;seal(b);rejects(b);b=good;b[12]=1;rejects(b);
    b=good;b[13]=5;seal(b);rejects(b);
    for(std::size_t size=0;size<64;++size)rejects(good,size);
    rejects(good,65);assert(!re6::decode_settings(nullptr,64,out));
    Bytes untouched{};untouched.fill(0xa5);const auto before=untouched;
    for(auto n:{2u,255u}){value={};value.language=static_cast<re6::Language>(n);assert(!re6::valid_settings(value));assert(!re6::encode_settings(value,untouched.data(),64) && untouched==before);}
    for(auto n:{5u,255u}){value={};value.texture_filtering=static_cast<re6::TextureFilter>(n);assert(!re6::valid_settings(value));assert(!re6::encode_settings(value,untouched.data(),64) && untouched==before);}
    for(auto n:{5u,255u}){value={};value.anti_aliasing=static_cast<re6::AntiAliasing>(n);assert(!re6::valid_settings(value));assert(!re6::encode_settings(value,untouched.data(),64) && untouched==before);}
    assert(!re6::encode_settings({},nullptr,64));
    assert(!re6::encode_settings({},untouched.data(),63) && untouched==before);
    assert(!re6::encode_settings({},untouched.data(),65) && untouched==before);
}
constexpr const char* path="build/re6-settings-test/settings.bin";
constexpr const char* previous="build/re6-settings-test/settings.bin.previous";
constexpr const char* incoming="build/re6-settings-test/settings.bin.incoming";
void write(const char* p,const void* bytes,std::size_t size){FILE* f=std::fopen(p,"wb");assert(f);assert(std::fwrite(bytes,1,size,f)==size);assert(!std::fclose(f));}
void cleanup(){for(const auto* p:{path,previous,incoming})std::filesystem::remove(p);}
void persistence(){
    std::filesystem::create_directories("build/re6-settings-test");cleanup();
    re6::Settings first,second,out;second.language=re6::Language::Russian;second.texture_filtering=re6::TextureFilter::Af8x;second.anti_aliasing=re6::AntiAliasing::Fxaa3High;
    assert(re6::save_settings_file(path,first) && std::filesystem::file_size(path)==64);
    assert(re6::save_settings_file(path,second) && re6::load_settings_file(path,out) && equal(second,out));
    auto bad=second;bad.texture_filtering=static_cast<re6::TextureFilter>(5);assert(!re6::save_settings_file(path,bad));
    assert(re6::load_settings_file(path,out) && equal(second,out));write(path,"bad",3);
    assert(re6::load_settings_file(path,out) && equal(first,out));assert(re6::save_settings_file(path,second));
    assert(std::filesystem::remove(path));assert(re6::load_settings_file(path,out) && equal(first,out));
    std::array<std::uint8_t,65> oversized{};assert(re6::encode_settings(second,oversized.data(),64));write(path,oversized.data(),65);
    assert(re6::load_settings_file(path,out) && equal(first,out));write(previous,"bad",3);out=second;
    assert(!re6::load_settings_file(path,out) && equal(second,out));
    assert(!re6::load_settings_file(nullptr,out) && !re6::load_settings_file("",out) && equal(second,out));
    assert(!re6::save_settings_file(nullptr,first) && !re6::save_settings_file("",first));
    cleanup();assert(re6::save_settings_file(path,first));assert(std::filesystem::create_directory(incoming));
    assert(!re6::save_settings_file(path,second));assert(re6::load_settings_file(path,out) && equal(first,out));assert(std::filesystem::remove(incoming));
    assert(std::filesystem::create_directory(previous));constexpr const char* blocker="build/re6-settings-test/settings.bin.previous/blocker";write(blocker,"keep",4);
    assert(!re6::save_settings_file(path,second));assert(re6::load_settings_file(path,out) && equal(first,out));assert(std::filesystem::file_size(blocker)==4);
    assert(std::filesystem::remove(blocker) && std::filesystem::remove(previous));
    assert(std::filesystem::remove(path) && std::filesystem::create_directory(path));
    assert(!re6::save_settings_file(path,second) && std::filesystem::is_directory(path));assert(std::filesystem::remove(path));
    out=second;assert(!re6::load_settings_file(path,out) && equal(second,out));cleanup();
}
}
int main(){wire();persistence();std::puts("RE6 graphics settings: native/all filter+AA modes, strict wire, save/recovery PASS");}
