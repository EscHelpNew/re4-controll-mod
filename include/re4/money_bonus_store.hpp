#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace re4 {
// Read-only compatibility with0.27.6. New receipts use file presence alone;
// save slots, aliases and campaign IDs no longer authorize or block a grant.
constexpr std::size_t kLegacyMoneyStoreBytes=33056;
inline bool legacy_money_claimed(const std::uint8_t* bytes,std::size_t size){
    if(!bytes||size!=kLegacyMoneyStoreBytes||std::memcmp(bytes,"R4MB",4))return false;
    std::uint32_t h[5]{};std::memcpy(h,bytes+4,sizeof(h));
    if(h[0]!=1||!h[1]||h[1]>256||h[2]>4096||!h[3])return false;
    for(unsigned i=24;i<32;++i)if(bytes[i])return false;
    bool claimed=false;
    for(unsigned i=0;i<256;++i){
        const auto flag=bytes[32+i];if(flag>1||(i>=h[1]&&flag))return false;
        claimed|=flag!=0;
    }
    std::uint32_t hash=2166136261u;
    for(std::size_t i=0;i<size;++i){hash^=i>=20&&i<24?0:bytes[i];hash*=16777619u;}
    return claimed&&hash==h[4];
}
}
