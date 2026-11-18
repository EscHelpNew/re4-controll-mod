#pragma once
#include <algorithm>
#include <cstdint>

namespace re4 {
// Canonical full-range ordinary hits use the regular native power row.
// Shotgun rays queue one target message; range falloff never changes units.
inline bool enemy_health_reference_weapon_allowed(unsigned index){
    switch(index){
        case 1:case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:
        case 11:case 12:case 15:case 17:case 33:return true;
        default:return false;
    }
}
inline bool enemy_health_reference_subtype_allowed(unsigned subtype){
    return subtype<=255;
}
inline bool enemy_health_reference_ranked_type(unsigned type){
    return type>=0x10&&type<=0x4F&&((0xFFFF97F7BF7FFFFFull>>(type-0x10))&1);
}
// Synthetic Firepower is always1.0. Actual upgrades, extra ownership, ammo,
// aimed part and previous hits deliberately cannot enter this calculation.
// Native float32 multiply/truncate order is preserved; no damage helper runs.
inline unsigned enemy_health_reference_damage(unsigned weapon,int coefficient,unsigned type,
        unsigned mode,unsigned rank,unsigned status,bool linked=false,unsigned phase=0,unsigned subtype=0){
    if(!enemy_health_reference_subtype_allowed(subtype)
        ||!enemy_health_reference_weapon_allowed(weapon)||coefficient<=0||coefficient>=9999
        ||type<0x10||type>0x4F||status>2||rank>10
        ||mode<2||mode>255)return 0;
    // Native84F0 is an unsignedbyte.1CCCC0 only branches for0/1's easy
    // double/random boost; every2..255 follows identical supported-gun math.
    // Enemy1D1520 reads actual500C rank without any84F0 mode condition.
    if(type==0x2B||type==0x2F||type==0x32||type==0x3F)return 0; // Undefined ordinary-body HP damage.
    const bool boss=type==0x2C||type==0x31||type==0x35||type==0x38||type==0x39;
    if(boss&&status)return 0; // Bosses do not use the ordinary family's B8C scaler.
    int damage=coefficient;
    // Native Saddler core bypasses /5 only for hit-shape selector0C.
    // Our fixed ordinary-body selector0 excludes that bonus for every weapon.
    if(type==0x31)damage/=5;
    if(status){
        const float factor=status==1?0.5555555820465088f:0.45454543828964233f;
        damage=int(float(damage)*factor)+1;
    }
    if(!boss){
        // Native90508..28: subtype2's separate two-thirds scaler followsB8C.
        if(subtype==2)damage=int(float(damage)*0.6666666865348816f)+1;
        // Shape0 A/D resistance jumps directly to the common tail and skips
        // linked-part scaling. Other252subtypes share the normal body path.
        if(subtype==0xA)damage=(damage>>3)+1;
        else if(subtype==0xD)damage=(damage>>5)+1;
        else if(linked)damage=int(float(damage)*0.6665999889373779f)+1;
        if(subtype==6)damage=0; // Native91F40..44: immune ordinary body.
        if(phase==4){
            // TMP halves INSTEAD OF1000cap; rifle15 bypasses thatcap.
            if(weapon==11)damage/=2;
            else if(weapon!=15)damage=std::min(damage,1000);
        }
        // Native91FA4..C0: subtype2/TMP always halves once more, including
        // after phase4's first halve. Positive integer truncation order matters.
        if(subtype==2&&weapon==11)damage/=2;
    }
    damage=std::min(damage,9999);
    if(enemy_health_reference_ranked_type(type)){
        const float factor=rank<6?2.f-float(rank)*0.20000000298023224f
            :1.f-float(rank-5)*0.029999999329447746f;
        damage=int(float(damage)*factor);
    }
    return damage>0?unsigned(damage):0;
}
}
