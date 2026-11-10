#include "re4/money_bonus_store.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
namespace {
void seal(std::vector<std::uint8_t>& bytes){
    std::uint32_t hash=2166136261u;
    for(std::size_t i=0;i<bytes.size();++i){hash^=i>=20&&i<24?0:bytes[i];hash*=16777619u;}
    std::memcpy(bytes.data()+20,&hash,4);
}
}
int main(){
    std::vector<std::uint8_t> bytes(33056);
    std::memcpy(bytes.data(),"R4MB",4);const std::uint32_t header[]={1,2,0,17};
    std::memcpy(bytes.data()+4,header,sizeof(header));seal(bytes);
    assert(!re4::legacy_money_claimed(bytes.data(),bytes.size()));
    bytes[33]=1;seal(bytes);assert(re4::legacy_money_claimed(bytes.data(),bytes.size()));
    bytes[32]=1;seal(bytes);assert(re4::legacy_money_claimed(bytes.data(),bytes.size()));
    assert(!re4::legacy_money_claimed(nullptr,bytes.size()));
    assert(!re4::legacy_money_claimed(bytes.data(),bytes.size()-1));
    for(auto offset:{0u,4u,8u,12u,16u,24u,31u,32u,34u,unsigned(bytes.size()-1)}){
        auto bad=bytes;bad[offset]^=2;
        assert(!re4::legacy_money_claimed(bad.data(),bad.size()));
    }
    for(auto pair:{std::pair<unsigned,unsigned>{4,2},{8,0},{9,2},{13,32},{16,0},{24,1},{32,2},{34,1}}){
        auto bad=bytes;bad[pair.first]=std::uint8_t(pair.second);seal(bad);
        assert(!re4::legacy_money_claimed(bad.data(),bad.size()));
    }
    std::puts("legacy money receipt migration tests passed");
}
