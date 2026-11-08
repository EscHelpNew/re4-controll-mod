#include "re4/character_digest.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
namespace {
void check(const std::string& text,const char* expected,unsigned chunk){
    re4::CharacterDigest digest;
    for(std::size_t p=0;p<text.size();p+=chunk)assert(digest.update(text.data()+p,std::min<std::size_t>(chunk,text.size()-p)));
    std::array<std::uint8_t,32> result{};assert(digest.finish(result.data()));
    char hex[65]{};for(unsigned i=0;i<32;++i)std::snprintf(hex+i*2,3,"%02x",unsigned(result[i]));
    assert(std::strcmp(hex,expected)==0);
    assert(!digest.update("x",1)&&!digest.finish(result.data()));
}
}
int main(){
    for(unsigned chunk:{1u,7u,63u,64u,65u,262144u}){
        check("","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",chunk);
        check("abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",chunk);
        check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq","248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",chunk);
        check(std::string(1000000,'a'),"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",chunk);
    }
    re4::CharacterDigest bad;std::array<std::uint8_t,32> out{};
    assert(!bad.update(nullptr,1)&&!bad.finish(nullptr));
    assert(bad.update(nullptr,0)&&bad.finish(out.data()));
    std::puts("character digest: SHA-256 known vectors, padding boundaries and chunked reads PASS");
}
