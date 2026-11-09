#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace re4 {
constexpr unsigned kMoneyCampaigns=256,kMoneySaveAliases=4096;
constexpr std::size_t kMoneyStoreBytes=32+kMoneyCampaigns+8*kMoneySaveAliases;
struct MoneySaveAlias {std::uint32_t crc=0;std::uint16_t campaign=0;std::uint8_t slot=0;};
struct MoneyStore {
    std::uint32_t generation=0,campaign_count=0,alias_count=0;
    std::array<std::uint8_t,kMoneyCampaigns> claimed{};
    std::array<MoneySaveAlias,kMoneySaveAliases> aliases{};
};
inline unsigned money_new_campaign(MoneyStore& store){
    return store.campaign_count<kMoneyCampaigns?++store.campaign_count:0;
}
inline unsigned money_find_campaign(const MoneyStore& store,unsigned slot,std::uint32_t crc){
    if(slot>=20)return 0;
    for(unsigned i=0;i<store.alias_count;++i){const auto& e=store.aliases[i];if(e.slot==slot&&e.crc==crc)return e.campaign;}
    return 0;
}
inline bool money_bind_save(MoneyStore& store,unsigned slot,std::uint32_t crc,unsigned campaign){
    if(slot>=20||!campaign||campaign>store.campaign_count)return false;
    const auto previous=money_find_campaign(store,slot,crc);
    if(previous)return previous==campaign;
    if(store.alias_count==kMoneySaveAliases)return false;
    store.aliases[store.alias_count++]={crc,std::uint16_t(campaign),std::uint8_t(slot)};return true;
}
inline bool money_claimed(const MoneyStore& store,unsigned campaign){
    return campaign&&campaign<=store.campaign_count&&store.claimed[campaign-1];
}
inline bool money_claim(MoneyStore& store,unsigned campaign){
    if(!campaign||campaign>store.campaign_count||money_claimed(store,campaign))return false;
    store.claimed[campaign-1]=1;return true;
}
inline std::uint32_t money_checksum(const std::uint8_t* bytes){
    std::uint32_t hash=2166136261u;
    for(std::size_t i=0;i<kMoneyStoreBytes;++i){hash^=i>=20&&i<24?0:bytes[i];hash*=16777619u;}return hash;
}
inline void money_encode(const MoneyStore& store,std::uint8_t* bytes){
    std::memset(bytes,0,kMoneyStoreBytes);std::memcpy(bytes,"R4MB",4);
    const std::uint32_t header[]={1,store.campaign_count,store.alias_count,store.generation};
    std::memcpy(bytes+4,header,sizeof(header));
    std::memcpy(bytes+32,store.claimed.data(),store.claimed.size());
    for(unsigned i=0;i<store.alias_count;++i){
        const auto& e=store.aliases[i];auto* p=bytes+32+kMoneyCampaigns+8*i;
        std::memcpy(p,&e.crc,4);std::memcpy(p+4,&e.campaign,2);p[6]=e.slot;
    }
    const auto sum=money_checksum(bytes);std::memcpy(bytes+20,&sum,4);
}
inline bool money_decode(const std::uint8_t* bytes,std::size_t size,MoneyStore& store){
    if(!bytes||size!=kMoneyStoreBytes||std::memcmp(bytes,"R4MB",4))return false;
    std::uint32_t h[5]{};std::memcpy(h,bytes+4,sizeof(h));
    if(h[0]!=1||h[1]>kMoneyCampaigns||h[2]>kMoneySaveAliases||h[4]!=money_checksum(bytes))return false;
    for(unsigned i=24;i<32;++i)if(bytes[i])return false;
    store={};store.campaign_count=h[1];store.alias_count=h[2];store.generation=h[3];
    for(unsigned i=0;i<kMoneyCampaigns;++i){
        const auto flag=bytes[32+i];if(flag>1||(i>=h[1]&&flag))return false;store.claimed[i]=flag;
    }
    for(unsigned i=0;i<kMoneySaveAliases;++i){
        const auto* p=bytes+32+kMoneyCampaigns+8*i;auto& e=store.aliases[i];
        std::memcpy(&e.crc,p,4);std::memcpy(&e.campaign,p+4,2);e.slot=p[6];
        if(p[7])return false;
        if(i>=h[2]){if(e.crc||e.campaign||e.slot)return false;continue;}
        if(!e.campaign||e.campaign>h[1]||e.slot>=20)return false;
        for(unsigned j=0;j<i;++j)if(store.aliases[j].slot==e.slot&&store.aliases[j].crc==e.crc)return false;
    }
    return true;
}
}
