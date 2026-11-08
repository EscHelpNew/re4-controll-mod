#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace re4 {
// One extra Firepower level beyond the native maximum. Values come from the
// authenticated native tables (main c4170af8…): firepower 0xAF0E8C, max levels
// 0xA08078 and tune-up prices 0xAFF638. Records always hold base raw ids.
// Mine Thrower (0x36) and Chicago Typewriter (0x34) gain nothing (last==previous)
// and are deliberately absent.
struct ExtendedUpgradeWeapon {
    std::uint16_t raw;
    std::uint8_t index;   // native weapon index (0x2939B0)
    std::uint8_t max;     // native maximum Firepower level (1-based)
    float previous,last;  // native values at max-1 and max
    std::uint32_t price;  // pesetas: the native price of the last Firepower level
};
inline constexpr ExtendedUpgradeWeapon kExtendedUpgradeWeapons[]={
    {0x23,2,6,1.8f,2.0f,20000},{0x25,3,6,3.0f,3.7f,45000},{0x03,17,6,2.3f,2.5f,35000},
    {0x21,1,6,1.7f,1.9f,35000},{0x27,4,6,2.7f,3.2f,40000},{0x29,5,6,24.f,28.f,70000},
    {0x2C,7,6,7.f,8.f,40000},{0x2D,8,6,10.f,12.f,60000},{0x94,33,6,7.f,8.f,50000},
    {0x2E,9,6,10.f,12.f,35000},{0x2F,10,6,13.f,15.f,40000},{0x30,11,6,1.0f,1.2f,35000},
    {0x2A,6,3,30.f,35.f,78000},{0x37,15,6,50.f,60.f,120000}};
inline constexpr unsigned kExtendedUpgradeCount=sizeof(kExtendedUpgradeWeapons)/sizeof(kExtendedUpgradeWeapons[0]);
static_assert(kExtendedUpgradeCount<=16,"ownership is a 16-bit mask");

inline int extended_upgrade_by_raw(unsigned raw){
    for(unsigned i=0;i<kExtendedUpgradeCount;++i)if(kExtendedUpgradeWeapons[i].raw==raw)return int(i);
    return -1;
}
// The extra level adds the native last step once more: E = last + (last-previous).
inline float extended_upgrade_value(const ExtendedUpgradeWeapon& w){return 2*w.last-w.previous;}
inline float extended_upgrade_ratio(const ExtendedUpgradeWeapon& w){return extended_upgrade_value(w)/w.last;}
// Packed record+6 low nibble is the 0-based Firepower level. Exclusive (max+1)
// also counts as maximal: the user chose to apply the same ratio on top of it.
inline bool extended_upgrade_at_max(const ExtendedUpgradeWeapon& w,std::uint32_t packed){
    return (packed&15u)+1u>=w.max;
}
inline bool extended_upgrade_owned(std::uint16_t mask,int entry){
    return entry>=0&&unsigned(entry)<kExtendedUpgradeCount&&(mask>>unsigned(entry)&1u);
}
// Native next-value digits: tenths split into hundreds/tens/ones, clamped to the
// three native digit sprites. Leading hundreds are hidden when zero.
struct ExtendedUpgradeDigits {unsigned hundreds=0,tens=0,ones=0;bool show_hundreds=false;};
inline bool extended_upgrade_digits(float value,ExtendedUpgradeDigits& out){
    if(!std::isfinite(value)||value<0)return false;
    float scaled=value*10.f+.5f;if(scaled>999.f)scaled=999.f;
    const unsigned tenths=unsigned(scaled);
    out={tenths/100%10,tenths/10%10,tenths%10,tenths>=100};
    return true;
}

// Native tune-up sprites live in a centered 640x480 space with Y up. The late
// unit list (29CEF0) displays it in the 512x448 HUD space with Y down.
struct ExtendedUpgradeRect {float left=0,top=0,right=0,bottom=0;};
// previous/last: world translations (sprite+14/+24) of the last two native
// squares; corners: the last square's local quad extent (sprite+BC..+E0).
// The seventh square repeats that exact quad one native pitch further right.
struct ExtendedUpgradeQuad {float min_x=0,min_y=0,max_x=0,max_y=0;};
inline bool extended_upgrade_square(float previous_x,float previous_y,float last_x,float last_y,
        ExtendedUpgradeQuad quad,ExtendedUpgradeRect& out){
    const float values[]={previous_x,previous_y,last_x,last_y,quad.min_x,quad.min_y,quad.max_x,quad.max_y};
    for(float v:values)if(!std::isfinite(v)||std::fabs(v)>2000.f)return false;
    const float pitch=last_x-previous_x;
    if(!(pitch>1.f&&pitch<80.f)||std::fabs(last_y-previous_y)>1.f)return false;
    if(!(quad.max_x-quad.min_x>.5f&&quad.max_x-quad.min_x<=pitch*1.5f)||!(quad.max_y-quad.min_y>.5f&&quad.max_y-quad.min_y<80.f))return false;
    const float cx=last_x+pitch,cy=last_y;
    const float sx=.8f,sy=448.f/480.f;
    out={(cx+quad.min_x+320.f)*sx,(240.f-(cy+quad.max_y))*sy,(cx+quad.max_x+320.f)*sx,(240.f-(cy+quad.min_y))*sy};
    return out.left>=0&&out.top>=0&&out.right<=512&&out.bottom<=448&&out.left<out.right&&out.top<out.bottom;
}

// Sidecar: per native save slot, the two most recent (slot CRC, ownership)
// pairs. A failed native write keeps the previous pair valid.
inline constexpr unsigned kExtendedUpgradeSlots=20;
inline constexpr std::size_t kExtendedUpgradeFileBytes=16+kExtendedUpgradeSlots*16;
struct ExtendedUpgradeEntry {std::uint32_t crc=0;std::uint16_t mask=0;bool used=false;};
struct ExtendedUpgradeStore {std::array<std::array<ExtendedUpgradeEntry,2>,kExtendedUpgradeSlots> slots{};};
inline std::uint32_t extended_upgrade_checksum(const std::uint8_t* p,std::size_t size){
    std::uint32_t hash=2166136261u;for(std::size_t i=0;i<size;++i){hash^=p[i];hash*=16777619u;}return hash;
}
inline void extended_upgrade_put(ExtendedUpgradeStore& store,unsigned slot,std::uint32_t crc,std::uint16_t mask){
    if(slot>=kExtendedUpgradeSlots)return;
    auto& e=store.slots[slot];
    if(e[0].used&&e[0].crc==crc){e[0].mask=mask;return;}
    e[1]=e[0];e[0]={crc,mask,true};
}
inline bool extended_upgrade_get(const ExtendedUpgradeStore& store,unsigned slot,std::uint32_t crc,std::uint16_t& mask){
    if(slot>=kExtendedUpgradeSlots)return false;
    for(const auto& e:store.slots[slot])if(e.used&&e.crc==crc){mask=e.mask;return true;}
    return false;
}
inline void extended_upgrade_encode(const ExtendedUpgradeStore& store,std::uint8_t (&out)[kExtendedUpgradeFileBytes]){
    std::memset(out,0,sizeof(out));std::memcpy(out,"R4XU",4);out[4]=1;
    for(unsigned s=0;s<kExtendedUpgradeSlots;++s)for(unsigned i=0;i<2;++i){
        const auto& e=store.slots[s][i];auto* p=out+16+s*16+i*8;
        std::memcpy(p,&e.crc,4);std::memcpy(p+4,&e.mask,2);p[6]=e.used?1:0;
    }
    const auto sum=extended_upgrade_checksum(out+16,sizeof(out)-16);std::memcpy(out+8,&sum,4);
}
inline bool extended_upgrade_decode(const std::uint8_t* data,std::size_t size,ExtendedUpgradeStore& out){
    if(!data||size!=kExtendedUpgradeFileBytes||std::memcmp(data,"R4XU",4)||data[4]!=1
        ||data[5]||data[6]||data[7]||data[12]||data[13]||data[14]||data[15])return false;
    std::uint32_t sum=0;std::memcpy(&sum,data+8,4);
    if(sum!=extended_upgrade_checksum(data+16,size-16))return false;
    ExtendedUpgradeStore store;
    for(unsigned s=0;s<kExtendedUpgradeSlots;++s)for(unsigned i=0;i<2;++i){
        const auto* p=data+16+s*16+i*8;auto& e=store.slots[s][i];
        if(p[6]>1||p[7])return false;
        std::memcpy(&e.crc,p,4);std::memcpy(&e.mask,p+4,2);e.used=p[6]!=0;
        if(!e.used&&(e.crc||e.mask))return false;
        if(e.mask>>kExtendedUpgradeCount)return false;
    }
    out=store;return true;
}
}
