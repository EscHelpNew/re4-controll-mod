#include "re4/extended_upgrade.hpp"
#include <cstdio>
#include <cstdlib>

#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"extended upgrade line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
using namespace re4;
namespace {
bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
}
int main(){
    // Native values from tables 0xAF0E8C/0xA08078/0xAFF638 (extra_level_table.txt).
    struct Expected {unsigned raw;float value;std::uint32_t price;};
    const Expected expected[]={{0x23,2.2f,20000},{0x25,4.4f,45000},{0x03,2.7f,35000},{0x21,2.1f,35000},
        {0x27,3.7f,40000},{0x29,32.f,70000},{0x2C,9.f,40000},{0x2D,14.f,60000},{0x94,9.f,50000},
        {0x2E,14.f,35000},{0x2F,17.f,40000},{0x30,1.4f,35000},{0x2A,40.f,78000},{0x37,70.f,120000}};
    CHECK(kExtendedUpgradeCount==14);
    for(const auto& e:expected){
        const int i=extended_upgrade_by_raw(e.raw);CHECK(i>=0);
        const auto& w=kExtendedUpgradeWeapons[i];
        CHECK(near(extended_upgrade_value(w),e.value)&&w.price==e.price&&w.last>w.previous);
        CHECK(near(extended_upgrade_ratio(w)*w.last,e.value)&&extended_upgrade_ratio(w)>1.f);
    }
    // Zero-gain weapons, grenades and unknown ids have no extra level.
    for(unsigned raw:{0x34u,0x36u,0xABu,0x01u,0x02u,0x0Eu,0x04u,0x10Fu})CHECK(extended_upgrade_by_raw(raw)<0);
    // Every weapon index is unique: damage resolves the entry by native index.
    for(unsigned a=0;a<kExtendedUpgradeCount;++a)for(unsigned b=a+1;b<kExtendedUpgradeCount;++b)
        CHECK(kExtendedUpgradeWeapons[a].index!=kExtendedUpgradeWeapons[b].index&&kExtendedUpgradeWeapons[a].raw!=kExtendedUpgradeWeapons[b].raw);
    // Maximum: nibble+1 >= max, so the native exclusive (nibble6) counts too.
    const auto& handgun=kExtendedUpgradeWeapons[extended_upgrade_by_raw(0x23)];
    for(unsigned nibble=0;nibble<16;++nibble)CHECK(extended_upgrade_at_max(handgun,0xFFFFFFF0u|nibble)==(nibble>=5));
    const auto& killer=kExtendedUpgradeWeapons[extended_upgrade_by_raw(0x2A)];
    CHECK(killer.max==3&&!extended_upgrade_at_max(killer,1)&&extended_upgrade_at_max(killer,2));
    CHECK(extended_upgrade_owned(1u<<3,3)&&!extended_upgrade_owned(1u<<3,2)&&!extended_upgrade_owned(0xFFFF,-1)&&!extended_upgrade_owned(0xFFFF,14));
    // Digits match native tenths splitting; three native digits clamp at 99.9.
    ExtendedUpgradeDigits d;
    CHECK(extended_upgrade_digits(2.2f,d)&&d.hundreds==0&&d.tens==2&&d.ones==2&&!d.show_hundreds);
    CHECK(extended_upgrade_digits(32.f,d)&&d.hundreds==3&&d.tens==2&&d.ones==0&&d.show_hundreds);
    CHECK(extended_upgrade_digits(116.55f,d)&&d.hundreds==9&&d.tens==9&&d.ones==9);
    CHECK(!extended_upgrade_digits(-1.f,d)&&!extended_upgrade_digits(NAN,d));
    // Seventh square: one pitch after the last native square, mapped 640x480 -> 512x448.
    ExtendedUpgradeRect r;const ExtendedUpgradeQuad quad{-8,-6,8,6};
    CHECK(extended_upgrade_square(100,-50,120,-50,quad,r));
    CHECK(near(r.left,(132.f+320.f)*.8f)&&near(r.right,(148.f+320.f)*.8f));
    CHECK(near(r.top,(240.f+44.f)*448.f/480.f)&&near(r.bottom,(240.f+56.f)*448.f/480.f));
    CHECK(!extended_upgrade_square(120,-50,100,-50,quad,r)&&!extended_upgrade_square(100,-50,120,-40,quad,r));
    CHECK(!extended_upgrade_square(300,0,319,0,quad,r)&&!extended_upgrade_square(NAN,0,1,0,quad,r));
    CHECK(!extended_upgrade_square(100,-50,120,-50,{0,0,0,0},r)&&!extended_upgrade_square(100,-50,120,-50,{-40,-6,40,6},r));
    // Sidecar: two newest CRCs per slot survive a failed native write.
    ExtendedUpgradeStore store;std::uint16_t mask=0;
    CHECK(!extended_upgrade_get(store,0,1,mask));
    extended_upgrade_put(store,4,0x1111,0x0003);extended_upgrade_put(store,4,0x2222,0x0007);
    CHECK(extended_upgrade_get(store,4,0x1111,mask)&&mask==3&&extended_upgrade_get(store,4,0x2222,mask)&&mask==7);
    extended_upgrade_put(store,4,0x2222,0x000F);CHECK(extended_upgrade_get(store,4,0x2222,mask)&&mask==15);
    CHECK(extended_upgrade_get(store,4,0x1111,mask)&&mask==3);
    extended_upgrade_put(store,4,0x3333,0x0001);CHECK(!extended_upgrade_get(store,4,0x1111,mask));
    CHECK(!extended_upgrade_get(store,5,0x2222,mask));
    extended_upgrade_put(store,20,1,1);extended_upgrade_put(store,19,0,0x2000);
    std::uint8_t bytes[kExtendedUpgradeFileBytes];extended_upgrade_encode(store,bytes);
    ExtendedUpgradeStore decoded;CHECK(extended_upgrade_decode(bytes,sizeof(bytes),decoded));
    CHECK(extended_upgrade_get(decoded,4,0x3333,mask)&&mask==1&&extended_upgrade_get(decoded,4,0x2222,mask)&&mask==15);
    CHECK(extended_upgrade_get(decoded,19,0,mask)&&mask==0x2000);
    for(std::size_t i=0;i<sizeof(bytes);++i){
        auto bad=bytes;bad[i]^=0x40;ExtendedUpgradeStore out;CHECK(!extended_upgrade_decode(bad,sizeof(bad),out));
    }
    CHECK(!extended_upgrade_decode(bytes,sizeof(bytes)-1,decoded)&&!extended_upgrade_decode(nullptr,sizeof(bytes),decoded));
    // A mask naming a non-existent entry is rejected even with a valid checksum.
    {
        ExtendedUpgradeStore wide;extended_upgrade_put(wide,0,9,std::uint16_t(1u<<14));
        std::uint8_t raw[kExtendedUpgradeFileBytes];extended_upgrade_encode(wide,raw);
        CHECK(!extended_upgrade_decode(raw,sizeof(raw),decoded));
    }
    std::puts("extended upgrade: native values, ownership, digits, square mapping and sidecar PASS");
}
