#pragma once
#include "re4/shared.hpp"
#include "re4/live_knife.hpp"
#include "re4/live_renderer.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace re4 {
constexpr std::size_t live_diagnostic_json_limit=16384;
// Readable observations deliberately retain collapsed/reversed/oversized
// rectangles. These values can explain missing rendering and are not visibility.
inline bool live_diagnostic_bounds_readable(const float* viewport,const std::int32_t* scissor){
    if(!viewport||!scissor)return false;
    const bool disabled=native_scissor_disabled(scissor);
    for(unsigned i=0;i<4;++i)
        if(!std::isfinite(viewport[i])||std::fabs(viewport[i])>65536
            ||(!disabled&&(scissor[i]< -65536||scissor[i]>65536)))return false;
    return true;
}
inline bool live_diagnostic_bounds_usable(const float* viewport,const std::int32_t* scissor){
    return live_diagnostic_bounds_readable(viewport,scissor)
        &&viewport[2]>0&&viewport[3]>0&&(native_scissor_disabled(scissor)||(scissor[2]>0&&scissor[3]>0));
}
enum class LiveDiagnosticStatus : std::uint8_t {
    NoGame, ServiceUnavailable, UnsupportedBuild, ModuleMissing, ReadFailed, Connected
};
// Fixed, copied observations only. No process addresses or file contents.
// Each subsection has its own validity; shared counters are a best-effort
// observation, while settings/appearance/ordered popup use independent seqlocks.
struct LiveDiagnosticSnapshot {
    LiveDiagnosticStatus status=LiveDiagnosticStatus::NoGame;
    bool process_found=false,supported=false,connected=false;
    SharedControl shared{};
    bool settings_valid=false,appearance_valid=false;
    Settings settings{};
    bool popup_cached_valid=false,popup_capture_stable=false,popup_capture_bounds_valid=false;
    std::array<float,4> popup_cached_viewport{};
    std::array<std::int32_t,4> popup_cached_scissor{};
    bool player_valid=false,camera_valid=false;
    std::array<std::uint8_t,4> player_routine{};
    std::uint8_t player_type=0,player_bones=0;
    std::array<float,12> camera_view{};
    std::array<float,16> camera_projection{};
    LiveKnifeObservation knife{};
    LiveHudRendererObservation hud_cached{};
};
// NUL-terminated JSON on success, exact byte count excludes NUL. Capacity is
// capped at16384; failures return size0 and an empty string when possible.
bool format_live_diagnostics(const LiveDiagnosticSnapshot&,char* output,
                             std::size_t capacity,std::size_t& size);
}
