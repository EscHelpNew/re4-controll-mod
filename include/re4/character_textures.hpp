#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace re4 {
// Raw native layouts: registry 64 x 0x10; descriptors 3000 x 0x40;
// parallel GPU object records 3000 x 0x18. The optional GPU view supplies
// no-op proof for sweep holes; missing proof requests renderer retirement.
// Read and execute together on the graphics worker using one coherent pin snapshot.
struct CharacterTextureTables {
    void* registry=nullptr;std::size_t registry_count=0;
    const void* descriptors=nullptr;std::size_t descriptor_count=0,sweep_descriptor_count=0;
    const void* gpu_objects=nullptr;std::size_t gpu_object_count=0;
};
struct CharacterTexturePins {std::array<std::uint32_t,2> ids{};std::size_t count=0;};
enum class CharacterTextureOperation {Exact,Group,MaskedGroup,SelectiveSweep,RendererTeardown};
struct CharacterTextureRequest {CharacterTextureOperation operation;std::uint32_t pack_or_group=0,delay=0,option_mask=0;};
enum class CharacterTextureAction {Native,Apply,RetireRenderer};
struct CharacterTexturePlan {
    CharacterTextureAction action=CharacterTextureAction::Native;
    std::array<std::uint32_t,3064> free_ids{};std::size_t free_count=0;
    std::array<std::uint8_t,64> delay_slots{};std::size_t delay_count=0;
    std::uint8_t delay=0;bool reset_cache=false,sweep_cleanup=false;
    CharacterTextureTables source{};
};
struct CharacterTextureCallbacks {
    void* context=nullptr;void (*exact_free)(void*,std::uint32_t)=nullptr;
    void (*reset_cache)(void*)=nullptr;void (*sweep_cleanup)(void*)=nullptr;
};
namespace character_texture_detail {
constexpr std::uint32_t absent=0xffffffffu;
template<class T> inline T read(const void* data,std::size_t offset){
    T value{};std::memcpy(&value,static_cast<const unsigned char*>(data)+offset,sizeof(value));return value;
}
inline bool pinned(const CharacterTexturePins& pins,std::uint32_t id){
    for(std::size_t i=0;i<pins.count;++i)if(pins.ids[i]==id)return true;
    return false;
}
inline bool preserved(std::uint32_t id){
    switch(id>>24){case 0x0f:case 0x18:case 0x46:case 0x49:case 0x4c:return true;default:return false;}
}
inline bool valid(const CharacterTextureTables& t){
    return t.registry_count<=64&&t.descriptor_count<=3000&&t.sweep_descriptor_count<=t.descriptor_count
        &&(!t.registry_count||t.registry)&&(!t.descriptor_count||t.descriptors)
        &&t.gpu_object_count<=3000&&(!t.gpu_object_count||t.gpu_objects);
}
inline bool same(const CharacterTextureTables& a,const CharacterTextureTables& b){
    return a.registry==b.registry&&a.registry_count==b.registry_count&&a.descriptors==b.descriptors
        &&a.descriptor_count==b.descriptor_count&&a.sweep_descriptor_count==b.sweep_descriptor_count
        &&a.gpu_objects==b.gpu_objects&&a.gpu_object_count==b.gpu_object_count;
}
inline bool descriptor_payload(const CharacterTextureTables& t,std::size_t i,bool ignore_owner=false){
    const auto* record=static_cast<const unsigned char*>(t.descriptors)+i*64;
    for(std::size_t j=0;j<64;++j)if((!ignore_owner||j<44||j>=48)&&record[j])return true;
    return false;
}
inline bool gpu_free_is_noop(const CharacterTextureTables& t,std::size_t i){
    // Native exact/sweep first tests the format/resource pointer at +8. If it
    // is null, +0/+10 are neither destroyed nor cleared, even if stale/nonzero.
    return i<t.gpu_object_count&&read<std::uint64_t>(t.gpu_objects,i*24+8)==0;
}
inline bool sweep_selects_id(const CharacterTextureTables& t,const CharacterTexturePins& pins,std::uint32_t id){
    if(id==absent||preserved(id)||pinned(pins,id))return false;
    for(std::size_t i=0;i<t.registry_count;++i)if(read<std::uint32_t>(t.registry,i*16+8)==id)return true;
    for(std::size_t i=0;i<t.sweep_descriptor_count;++i)if(read<std::uint32_t>(t.descriptors,i*64+44)==id)return true;
    return false;
}
inline void add(CharacterTexturePlan& plan,std::uint32_t id,const CharacterTexturePins& pins){
    if(id==absent||pinned(pins,id))return;
    for(std::size_t i=0;i<plan.free_count;++i)if(plan.free_ids[i]==id)return;
    // At most one ID per bounded source record: 64 + 3000.
    plan.free_ids[plan.free_count++]=id;
}
}
// Native means call the original request unchanged. RetireRenderer is NOT permission
// to call it yet: invalidate appearance, complete renderer retirement/fence, then
// perform native teardown/cleanup under a new renderer generation. Never unpin here.
inline CharacterTexturePlan plan_character_textures(CharacterTextureTables tables,CharacterTexturePins pins,CharacterTextureRequest request){
    using namespace character_texture_detail;
    CharacterTexturePlan plan;plan.source=tables;
    const auto retire=[&](){plan.action=CharacterTextureAction::RetireRenderer;return plan;};
    if(pins.count>pins.ids.size())return retire();
    for(std::size_t i=0;i<pins.count;++i)if(pins.ids[i]==absent)return retire();
    if(request.operation==CharacterTextureOperation::RendererTeardown)return retire();
    if(request.operation==CharacterTextureOperation::Exact){
        if(pinned(pins,request.pack_or_group))plan.action=CharacterTextureAction::Apply;
        return plan;
    }
    const bool sweep=request.operation==CharacterTextureOperation::SelectiveSweep;
    const bool masked=request.operation==CharacterTextureOperation::MaskedGroup;
    if(!sweep&&!masked&&request.operation!=CharacterTextureOperation::Group)return retire();
    const auto group=static_cast<std::uint8_t>(request.pack_or_group);
    bool affected=false;
    for(std::size_t i=0;i<pins.count;++i)
        affected|=sweep?!preserved(pins.ids[i]):(pins.ids[i]>>24)==group;
    if(!affected)return plan;
    if(!valid(tables))return retire();
    if(sweep){
        // Owner FFFFFFFF cannot be safely exact-freed: that also matches stale
        // registry slots. Only an otherwise empty descriptor and a null native
        // +8 GPU resource pointer prove there is no destruction to reproduce.
        // Its invalid owner sentinel may remain; no pack/resource is retained.
        for(std::size_t i=0;i<tables.sweep_descriptor_count;++i)
            if(read<std::uint32_t>(tables.descriptors,i*64+44)==absent
                &&(descriptor_payload(tables,i,true)||!gpu_free_is_noop(tables,i)))return retire();
        // Exact-ID expansion must not destroy cache entries outside the native
        // sweep prefix. All-zero descriptors with proven no-op GPU records are
        // ordinary unused holes and need no retirement (notably pack ID zero).
        for(std::size_t i=tables.sweep_descriptor_count;i<tables.descriptor_count;++i){
            const auto id=read<std::uint32_t>(tables.descriptors,i*64+44);
            if((descriptor_payload(tables,i)||!gpu_free_is_noop(tables,i))&&sweep_selects_id(tables,pins,id))return retire();
        }
    }
    plan.action=CharacterTextureAction::Apply;
    plan.delay=sweep?0:static_cast<std::uint8_t>(request.delay);
    // Group FF includes stale native absent slots. No safe exact-ID expansion can
    // free those while preserving pins; it requires retirement/native handling.
    if(!sweep&&group==0xff&&!plan.delay)return retire();
    const auto mask=static_cast<std::uint16_t>(request.option_mask);
    if(masked&&!plan.delay){
        for(std::size_t i=0;i<tables.registry_count;++i){
            const auto id=read<std::uint32_t>(tables.registry,i*16+8);
            if(id==absent||pinned(pins,id)||(id>>24)!=group)continue;
            const bool selected=(read<std::uint16_t>(tables.registry,i*16+12)&mask)!=0;
            for(std::size_t j=0;j<i;++j)if(read<std::uint32_t>(tables.registry,j*16+8)==id)
                if(selected!=((read<std::uint16_t>(tables.registry,j*16+12)&mask)!=0))return retire();
        }
    }
    for(std::size_t i=0;i<tables.registry_count;++i){
        const auto id=read<std::uint32_t>(tables.registry,i*16+8);
        const bool selected=sweep?(id!=absent&&!preserved(id)):
            ((id>>24)==group&&(!masked||(read<std::uint16_t>(tables.registry,i*16+12)&mask)!=0));
        if(!selected||pinned(pins,id))continue;
        if(plan.delay)plan.delay_slots[plan.delay_count++]=static_cast<std::uint8_t>(i);
        else add(plan,id,pins);
    }
    if(!plan.delay&&!masked){
        const auto count=sweep?tables.sweep_descriptor_count:tables.descriptor_count;
        for(std::size_t i=0;i<count;++i){
            const auto id=read<std::uint32_t>(tables.descriptors,i*64+44);
            if(sweep?!preserved(id):(id>>24)==group)add(plan,id,pins);
        }
    }
    plan.reset_cache=!plan.delay;plan.sweep_cleanup=sweep;
    return plan;
}
// exact_free MUST be the original/trampoline 23F4F0, not the pin hook recursively.
// reset_cache adapts context to 26F430's GPU context. sweep_cleanup must perform
// native 2F4360 + memset(11837F0,0,0x960) + reset *(GOT B26EA0), in that order.
// Callbacks never unpin or replay an earlier suppressed native free.
inline bool apply_character_texture_plan(const CharacterTexturePlan& plan,CharacterTextureTables tables,CharacterTextureCallbacks callbacks){
    if(plan.action!=CharacterTextureAction::Apply||!character_texture_detail::same(plan.source,tables)
        ||plan.free_count>plan.free_ids.size()||plan.delay_count>plan.delay_slots.size()
        ||(plan.free_count&&!callbacks.exact_free)||(plan.reset_cache&&!callbacks.reset_cache)
        ||(plan.sweep_cleanup&&!callbacks.sweep_cleanup))return false;
    for(std::size_t i=0;i<plan.delay_count;++i)
        if(!tables.registry||plan.delay_slots[i]>=tables.registry_count)return false;
    if(plan.reset_cache)callbacks.reset_cache(callbacks.context);
    for(std::size_t i=0;i<plan.delay_count;++i)
        static_cast<unsigned char*>(tables.registry)[std::size_t(plan.delay_slots[i])*16+14]=plan.delay;
    for(std::size_t i=0;i<plan.free_count;++i)callbacks.exact_free(callbacks.context,plan.free_ids[i]);
    if(plan.sweep_cleanup)callbacks.sweep_cleanup(callbacks.context);
    return true;
}
}
