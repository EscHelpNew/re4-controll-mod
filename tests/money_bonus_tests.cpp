#include "re4/money_bonus_store.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
int main(){
    re4::MoneyStore store;
    const auto first=re4::money_new_campaign(store);assert(first==1);
    assert(re4::money_bind_save(store,3,0xC0FFEE,first));
    assert(re4::money_bind_save(store,9,0x12345678,first));
    assert(re4::money_find_campaign(store,3,0xC0FFEE)==1);
    assert(re4::money_find_campaign(store,3,0xBAD)==0);
    assert(re4::money_find_campaign(store,20,0xC0FFEE)==0);
    assert(re4::money_claim(store,first)&&!re4::money_claim(store,first));
    assert(!re4::money_claim(store,0)&&!re4::money_claim(store,256));
    assert(!re4::money_bind_save(store,20,0,first)&&!re4::money_bind_save(store,0,0,0));
    assert(re4::money_bind_save(store,3,0xC0FFEE,first)&&store.alias_count==2);
    const auto second=re4::money_new_campaign(store);assert(second==2&&!re4::money_claimed(store,second));
    assert(!re4::money_bind_save(store,3,0xC0FFEE,second)); // Never reassign an older campaign.
    store.generation=17;
    std::vector<std::uint8_t> bytes(re4::kMoneyStoreBytes);
    re4::money_encode(store,bytes.data());re4::MoneyStore loaded;
    assert(re4::money_decode(bytes.data(),bytes.size(),loaded));
    assert(loaded.generation==17&&re4::money_claimed(loaded,1)&&!re4::money_claimed(loaded,2));
    assert(re4::money_find_campaign(loaded,9,0x12345678)==1);
    for(auto offset:{0u,4u,8u,16u,20u,31u,32u,300u,unsigned(bytes.size()-1)}){
        auto corrupt=bytes;corrupt[offset]^=1;assert(!re4::money_decode(corrupt.data(),corrupt.size(),loaded));
    }
    assert(!re4::money_decode(nullptr,bytes.size(),loaded));
    assert(!re4::money_decode(bytes.data(),bytes.size()-1,loaded));
    const auto malformed=[&](unsigned offset,unsigned value){
        auto corrupt=bytes;corrupt[offset]=std::uint8_t(value);
        const auto checksum=re4::money_checksum(corrupt.data());std::memcpy(corrupt.data()+20,&checksum,4);
        assert(!re4::money_decode(corrupt.data(),corrupt.size(),loaded));
    };
    for(auto pair:{std::pair<unsigned,unsigned>{4,2},{9,2},{13,32},{24,1},{32,2},{34,1},
                  {292,0},{293,1},{294,20},{295,1},{304,1}})malformed(pair.first,pair.second);
    auto duplicate=bytes;std::memcpy(duplicate.data()+296,duplicate.data()+288,8);
    const auto duplicate_sum=re4::money_checksum(duplicate.data());std::memcpy(duplicate.data()+20,&duplicate_sum,4);
    assert(!re4::money_decode(duplicate.data(),duplicate.size(),loaded));
    while(store.campaign_count<re4::kMoneyCampaigns)assert(re4::money_new_campaign(store));
    assert(!re4::money_new_campaign(store));
    while(store.alias_count<re4::kMoneySaveAliases)assert(re4::money_bind_save(store,0,store.alias_count+100,first));
    assert(!re4::money_bind_save(store,1,123,first));
    re4::money_encode(store,bytes.data());assert(re4::money_decode(bytes.data(),bytes.size(),loaded));
    std::puts("money campaign persistence tests passed");
}
