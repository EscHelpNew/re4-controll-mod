// Included immediately after EnemyHealthReader, inside module.cpp's namespace.
// Include reference/families/health_capture headers at the translation unit's top level.
#include "re4/health_capture.hpp"
struct EnemyHealthGuardEntry {
    std::atomic<unsigned> state{0}; // Unchecked, validating, accepted, rejected.
    re4::health_profile::Mismatch mismatch{};
};
struct EnemyHealthGuardCache {
    std::atomic<std::uintptr_t> base{0};std::atomic<std::size_t> size{0};
    EnemyHealthGuardEntry entries[re4::health_profile::kClassCount];
} enemy_health_guards;
void enemy_health_guard_bind(const std::uint8_t* memory,std::size_t size){
    enemy_health_guards.base.store(0,std::memory_order_release);
    for(auto& entry:enemy_health_guards.entries){entry.mismatch={};entry.state.store(0,std::memory_order_relaxed);}
    enemy_health_guards.size.store(size,std::memory_order_relaxed);
    enemy_health_guards.base.store(reinterpret_cast<std::uintptr_t>(memory),std::memory_order_release);
}
bool enemy_health_guard_accept(const re4::EnemyHealthClass& native_class,re4::HealthCaptureSample* observation){
    const auto which=re4::health_profile::class_index(native_class.vptr);
    const auto base=enemy_health_guards.base.load(std::memory_order_acquire);
    if(which>=re4::health_profile::kClassCount||!base||base!=main_base)return false;
    auto& entry=enemy_health_guards.entries[which];
    unsigned state=entry.state.load(std::memory_order_acquire);
    if(state==0){
        unsigned unchecked=0;
        if(entry.state.compare_exchange_strong(unchecked,1,std::memory_order_acq_rel)){
            re4::health_profile::Mismatch mismatch;
            const bool matches=re4::health_profile::class_matches(reinterpret_cast<const std::uint8_t*>(base),
                enemy_health_guards.size.load(std::memory_order_relaxed),which,&mismatch);
            entry.mismatch=mismatch;
            state=matches?2u:3u;entry.state.store(state,std::memory_order_release);
        }else state=unchecked;
    }
    if(state==2)return true;
    if(state==3&&observation){
        const auto& mismatch=entry.mismatch;using F=re4::HealthCaptureField;
        if(mismatch.kind==re4::health_profile::MismatchKind::Word
            ||mismatch.kind==re4::health_profile::MismatchKind::Bounds){
            observation->set(F::GuardRva,mismatch.rva);observation->set(F::GuardExpected,mismatch.expected);
            if(mismatch.kind==re4::health_profile::MismatchKind::Word)observation->set(F::GuardObserved,mismatch.actual);
        }
    }
    return false; // Another validator never makes gameplay wait.
}

bool enemy_health_reference_weapon(EnemyHealthReader& reader,unsigned& index,re4::HealthCaptureSample* observation=nullptr){
    std::uintptr_t state=0,actor=0,context=0,weapon=0;
    std::uint8_t number=0;
    if(!main_base||!reader.field(main_base,0xB1AE08,state)||!state
        ||!reader.field(main_base,re4::profile::player_slot,actor)||!actor
        ||!reader.field(actor,0x938,context)||!context
        ||!reader.field(context,0x38,weapon)||!weapon
        ||!reader.field(state,0x5034,number))return false;
    if(observation)observation->set(re4::HealthCaptureField::WeaponIndex,number);
    if(!number||number>0x30)return false;
    // Native296D78..296D8C stores2939B0(equipped effective ID) at5034.
    // It is the native weapon index, independent of the cached power at5037.
    index=number;return true;
}
std::uint64_t enemy_health_current_weapon_stamp(){
    EnemyHealthReader reader;unsigned index=0;
    return enemy_health_reference_weapon(reader,index)?std::uint64_t(index)+1:0;
}
bool enemy_health_reference_core_links(EnemyHealthReader& reader,std::uintptr_t core,
        std::uintptr_t vptr,unsigned type,std::uintptr_t pool,unsigned count,unsigned stride){
    const unsigned outward=type==0x31?0xBC4:0xAE4,reciprocal=type==0x31?0xBCC:0xAFC;
    std::uintptr_t body=0,back=0,body_vptr=0,again=0;
    unsigned flags=0;std::uint8_t body_type=0,subtype=0;
    return reader.field(core,outward,body)&&body!=core
        &&re4::enemy_health_pool_member(pool,count,stride,body)
        &&reader.field(body,0,body_vptr)&&body_vptr==vptr
        &&reader.field(body,8,flags)&&(flags&0x201)==1
        &&reader.field(body,0x118,body_type)&&body_type==type
        &&reader.field(body,0x119,subtype)&&subtype==0
        &&reader.field(body,reciprocal,back)&&back==core
        &&reader.field(core,outward,again)&&again==body;
}
unsigned enemy_health_body_reference(EnemyHealthReader& reader,std::uintptr_t target,re4::HealthCaptureSample* observation=nullptr){
    using F=re4::HealthCaptureField;using G=re4::HealthReportGate;
    const auto observe=[&](F field,std::uint32_t value){if(observation)observation->set(field,value);};
    const auto fail=[&](G gate){observe(F::FailedGate,unsigned(gate));return 0u;};
    const auto scalar=[&](std::uintptr_t base,std::size_t offset,auto& value,F field){
        const bool read=reader.field(base,offset,value);if(read)observe(field,std::uint32_t(value));return read;
    };
    const auto rva=[&](std::uintptr_t pointer){
        return pointer>=main_base&&pointer-main_base<0x1281000?std::uint32_t(pointer-main_base):UINT32_MAX;
    };
    observe(F::FailedGate,unsigned(G::None));
    unsigned weapon=0;
    if(!enemy_health_reference_weapon(reader,weapon,observation))return fail(G::Weapon);
    if(main_base>std::numeric_limits<std::uintptr_t>::max()-0x1281000)return fail(G::Context);
    std::uintptr_t pool=0,vptr=0,update=0,state=0,first=0,second=0;
    unsigned count=0,stride=0,flags=0;
    std::uint8_t type=0,subtype=0,status=0,phase=0,mode=0,rank=0;
    std::int16_t hp=0,maximum=0;
    if(!reader.field(main_base,0xAE6EF8+8,pool)
        ||!scalar(main_base,0xAE6EF8+0x10,count,F::PoolCount)
        ||!scalar(main_base,0xAE6EF8+0x14,stride,F::PoolStride))return fail(G::Pool);
    if(!re4::enemy_health_pool_member(pool,count,stride,target))return fail(G::Target);
    if(!reader.field(target,0,vptr))return fail(G::Class);
    observe(F::VptrRva,rva(vptr));
    if(!reader.field(vptr,0x28,update))return fail(G::Class);
    observe(F::UpdateRva,rva(update));
    if(!scalar(target,8,flags,F::Flags)||(flags&0x201)!=1
        ||!scalar(target,0x118,type,F::Type)||type<0x10||type>0x4F)return fail(G::Class);
    if(!reader.field(target,0x3C2,hp)||!reader.field(target,0x3C4,maximum))return fail(G::Health);
    observe(F::Health,std::uint32_t(std::uint16_t(hp))|(std::uint32_t(std::uint16_t(maximum))<<16));
    if(!re4::enemy_health_values(hp,maximum))return fail(G::Health);
    if(!scalar(target,0x119,subtype,F::Subtype))return fail(G::Subtype);
    using Family=re4::EnemyHealthFamily;
    const auto* native_class=re4::enemy_health_reference_class(rva(vptr),rva(update));
    if(!native_class)return fail(G::Class);
    const auto family=native_class->family;
    if(!re4::enemy_health_family_weapon_allowed(family,weapon))return fail(G::Weapon);
    if(!enemy_health_guard_accept(*native_class,observation))return fail(G::Class);
    bool core=false;
    if(family==Family::LegacyBoss){
        // These linked/typed boss policies remain qualified as their exact
        // original classes; a matching numeric118 alone never selects them.
        std::uintptr_t expected=0;
        switch(type){
            case 0x2C:expected=0xAE9438;if(subtype>1)return fail(G::Subtype);break;
            case 0x35:expected=0xAEAFD0;if(subtype>1)return fail(G::Subtype);break;
            case 0x39:expected=0xAEBF38;if(subtype>1)return fail(G::Subtype);break;
            case 0x31:expected=0xAEA170;if(subtype!=1)return fail(G::Subtype);core=true;break;
            case 0x38:expected=0xAEBC60;if(subtype!=3)return fail(G::Subtype);core=true;break;
            default:return fail(G::Class);
        }
        if(native_class->vptr!=expected)return fail(G::Class);
    }else if(family==Family::Common){
        if(!re4::enemy_health_reference_subtype_allowed(subtype))return fail(G::Subtype);
        if(!scalar(target,0xB8C,status,F::BodyStatus)||status>2
            ||!reader.field(target,0x944,first)||!reader.field(target,0x974,second))return fail(G::BodyStatus);
        observe(F::LinkMask,unsigned(first!=0)|(unsigned(second!=0)<<1));
        if(!scalar(target,0xB4C,phase,F::Phase))return fail(G::BodyStatus);
    }
    if((!core&&(flags&0x10000000))
        ||(core&&!enemy_health_reference_core_links(reader,target,vptr,type,pool,count,stride)))return fail(G::Class);
    const bool coefficient_needed=re4::enemy_health_family_needs_coefficient(family);
    const bool rank_needed=re4::enemy_health_family_needs_rank(family);
    const bool state_read=reader.field(main_base,0xB1AE08,state)&&state;
    if(coefficient_needed){
        if(!state_read)return fail(G::Context);
        if(!scalar(state,0x84F0,mode,F::Mode)||!scalar(state,0x500C,rank,F::Rank))return fail(G::ModeRank);
    }else if(state_read){
        // Direct/fixed handlers do not use the power calculator's mode. Keep
        // successful observations, but require only the inputs their math uses.
        scalar(state,0x84F0,mode,F::Mode);
        const bool rank_read=scalar(state,0x500C,rank,F::Rank);
        if(rank_needed&&!rank_read)return fail(G::ModeRank);
    }else if(rank_needed)return fail(G::Context);
    int coefficient=0;
    if(coefficient_needed){
        std::uintptr_t table=main_base+0xAF055C;
        if(type>=0x23&&type<=0x4E){
            if(!reader.field(main_base,0xAF1440+std::size_t(type-0x23)*8,table))return fail(G::Coefficient);
            observe(F::TableRva,rva(table));
            const auto base=main_base+0xAF055C;
            if(table<base||(table-base)%0xC4||(table-base)/0xC4>=12)return fail(G::Coefficient);
        }else observe(F::TableRva,rva(table));
        if(!scalar(table,std::size_t(weapon)*4,coefficient,F::Coefficient))return fail(G::Coefficient);
    }
    const auto unit=re4::enemy_health_family_damage(family,weapon,coefficient,type,mode,rank,status,first||second,phase,subtype,unsigned(maximum));
    if(!unit){
        if(coefficient_needed&&(coefficient<0||coefficient>=9999))return fail(G::Coefficient);
        if((coefficient_needed&&mode<2)||(rank_needed&&rank>10))return fail(G::ModeRank);
        return fail(G::Prediction);
    }
    return unit;
}
