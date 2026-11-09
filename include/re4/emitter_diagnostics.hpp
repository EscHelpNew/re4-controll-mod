#pragma once
#include "re4/shared.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace re4 {
constexpr std::size_t kEmitterDiagnosticsOffset=(sizeof(SharedControl)+15u)&~std::size_t(15u);
constexpr unsigned kEmitterDiagnosticsChannels=4,kEmitterDiagnosticsWords=20,kEmitterDiagnosticsBytes=352;
constexpr char kEmitterDiagnosticsMarker[16]="RE4CTL_EMIT1";
enum class EmitterDiagnosticResult : unsigned {
    None,Applied,LockBusy,Identity,Owner,Matrix,Pose,NativeInput,
    EffectPool,EffectState,EffectOwner,EffectGeneration,EffectPart,EffectGroup,EffectScript
};
enum class EmitterDiagnosticOwner : unsigned {None,Parent,Archive,Chain,Frame,PartRegion,Descriptor,Owned};
// Optional, pointer-free sidecar. The core shared ABI remains unchanged.
struct EmitterDiagnosticsWire {
    char marker[16];
    std::uint32_t version,bytes,sequence,channel_count;
    std::uint32_t records[kEmitterDiagnosticsChannels][kEmitterDiagnosticsWords];
};
constexpr EmitterDiagnosticsWire initial_emitter_diagnostics(){
    EmitterDiagnosticsWire value{};
    for(unsigned i=0;i<16;++i)value.marker[i]=kEmitterDiagnosticsMarker[i];
    value.version=1;value.bytes=kEmitterDiagnosticsBytes;value.channel_count=kEmitterDiagnosticsChannels;
    return value;
}
inline bool emitter_diagnostics_header_valid(const EmitterDiagnosticsWire& value){
    return !std::memcmp(value.marker,kEmitterDiagnosticsMarker,16)&&value.version==1
        &&value.bytes==kEmitterDiagnosticsBytes&&value.channel_count==kEmitterDiagnosticsChannels;
}
static_assert(sizeof(EmitterDiagnosticsWire)==kEmitterDiagnosticsBytes);
static_assert(kEmitterDiagnosticsOffset==512);
static_assert(std::is_standard_layout_v<EmitterDiagnosticsWire>&&std::is_trivially_copyable_v<EmitterDiagnosticsWire>);
}
