#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace re4 {
enum class TextureFilter : std::uint8_t { Native=0, Bilinear, Trilinear, Aniso2, Aniso4, Aniso8, Aniso16 };
enum class ShadowMode : std::uint8_t { Native=0, Off };
enum class AntiAliasing : std::uint8_t { Native=0, HandheldSsaa };
struct GraphicsOptions {
    TextureFilter filter=TextureFilter::Native;
    ShadowMode shadows=ShadowMode::Native;
    AntiAliasing aa=AntiAliasing::Native;
    float draw_distance=1;
};
inline bool valid_graphics(const GraphicsOptions& options) {
    return unsigned(options.filter)<=unsigned(TextureFilter::Aniso16)
        && unsigned(options.shadows)<=unsigned(ShadowMode::Off)
        && unsigned(options.aa)<=unsigned(AntiAliasing::HandheldSsaa)
        && std::isfinite(options.draw_distance) && options.draw_distance>=0.5f && options.draw_distance<=2;
}
inline std::uint64_t pack_graphics(const GraphicsOptions& options,bool ready=true) {
    const GraphicsOptions value=valid_graphics(options)?options:GraphicsOptions{};
    std::uint32_t distance;static_assert(sizeof(distance)==sizeof(value.draw_distance));
    std::memcpy(&distance,&value.draw_distance,sizeof(distance));
    return std::uint64_t(distance)<<32 | std::uint64_t(ready)<<24
        | std::uint64_t(value.aa)<<16 | std::uint64_t(value.shadows)<<8 | std::uint64_t(value.filter);
}
inline GraphicsOptions unpack_graphics(std::uint64_t word) {
    if(word&0xfe000000ull)return {};
    GraphicsOptions value{static_cast<TextureFilter>(word&255),static_cast<ShadowMode>((word>>8)&255),
        static_cast<AntiAliasing>((word>>16)&255),1};
    const auto distance=std::uint32_t(word>>32);std::memcpy(&value.draw_distance,&distance,sizeof(distance));
    return valid_graphics(value)?value:GraphicsOptions{};
}
inline bool graphics_ready(std::uint64_t word) {return (word&0x01000000ull)!=0;}

// NVN sampler descriptors are copied synchronously by the native constructor.
// Copy through local storage so in-place and overlapping descriptors are safe.
inline bool filter_sampler_descriptor(const std::uint8_t* src,std::uint8_t* out,std::size_t size,TextureFilter filter) {
    if(!src || !out || size<20)return false;
    std::uint8_t value[20];std::memcpy(value,src,sizeof(value));
    const unsigned flags=unsigned(value[6]) | unsigned(value[7])<<8;
    const bool eligible=((flags>>4)&3)==2 && ((flags>>2)&3)==2
        && ((flags&3)==1 || (flags&3)==2) && !(flags&0x80) && !(flags&0x300);
    if(eligible && filter!=TextureFilter::Native && unsigned(filter)<=unsigned(TextureFilter::Aniso16)) {
        const unsigned low=filter==TextureFilter::Bilinear?0x29:filter==TextureFilter::Trilinear?0x2a:0x6a;
        value[6]=std::uint8_t((flags&0x80)|low);
        value[5]=filter==TextureFilter::Aniso2?2:filter==TextureFilter::Aniso4?4:
            filter==TextureFilter::Aniso8?8:filter==TextureFilter::Aniso16?16:1;
    }
    const bool changed=std::memcmp(value,src,sizeof(value))!=0;
    std::memcpy(out,value,sizeof(value));return changed;
}
constexpr std::uint32_t kShadowDisableMask=0x02048000u;
inline std::uint32_t restore_shadow_flags(std::uint32_t current,std::uint32_t saved) {
    return (current&~kShadowDisableMask)|(saved&kShadowDisableMask);
}
inline std::uint32_t shadow_flags(ShadowMode mode,std::uint32_t original) {
    return mode==ShadowMode::Off?original|kShadowDisableMask:original;
}
inline float culling_distance(float native_far,float near_plane,float projection_far,float scale) {
    if(scale==1)return native_far;
    if(!std::isfinite(native_far) || !std::isfinite(near_plane) || !std::isfinite(projection_far)
        || !std::isfinite(scale) || near_plane<0 || native_far<=near_plane || projection_far<=near_plane
        || scale<0.5f || scale>2)return native_far;
    const double lower=double(near_plane)+1;
    if(double(projection_far)<lower)return native_far;
    const double scaled=double(native_far)*double(scale);
    return static_cast<float>(scaled<lower?lower:scaled>double(projection_far)?double(projection_far):scaled);
}
struct ResolutionState {int applied_display,applied_back,requested_display,requested_back;};
struct ResolutionPlan {bool request=false;int display=0,back=0;bool owned=false;};
inline ResolutionPlan plan_resolution(AntiAliasing mode,ResolutionState state,bool owned) {
    ResolutionPlan plan;plan.owned=owned;
    const auto valid_display=[](int index){return index==0 || index==2;};
    const auto valid_back=[](int index){return index>=0 && index<=3;};
    if(unsigned(mode)>unsigned(AntiAliasing::HandheldSsaa) || !valid_display(state.applied_display)
        || !valid_display(state.requested_display) || !valid_back(state.applied_back)
        || !valid_back(state.requested_back) || state.applied_display!=state.requested_display)return plan;
    if(state.applied_display==0){plan.owned=false;return plan;}
    if(mode==AntiAliasing::Native && !owned)return plan;
    plan.owned=mode==AntiAliasing::HandheldSsaa;
    const int desired=plan.owned?1:3;
    if(state.requested_back!=desired){plan.request=true;plan.display=2;plan.back=desired;}
    return plan;
}
inline int resolution_back_index(AntiAliasing mode,int width,int height,int native_display,int native_back) {
    return mode==AntiAliasing::HandheldSsaa && width==1280 && height==720 && native_display==2 && native_back==3
        ?1:native_back;
}
}
