#pragma once
#include "re4/shared.hpp"
#include "re4/live_knife.hpp"
#include "re4/live_renderer.hpp"
#include "re4/emitter_diagnostics.hpp"
#include "re4/health_report.hpp"
#include "re4/health_capture.hpp"
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
    NoGame, ServiceUnavailable, UnsupportedBuild, ModuleMissing, ReadFailed, Connected, DiscoveryLimited
};
// Failed initialization is an observation, never a connection or writable control block.
struct ModuleObservation {
    bool found=false,stable=false,scan_limit=false;
    std::uint32_t abi=0,status=0,capabilities=0,scan_reads=0,scan_bytes=0;
    std::uint64_t frames=0;
    std::array<char,16> version{};
};
struct ProfileMismatch {
    bool pointer=false,readable=false,in_bounds=false,observed_in_main=false,observed_null=false;
    std::uint32_t rva=0,expected=0,observed=0;
};
struct ProfileObservation {
    bool attempted=false,stable=false,complete=false,budget_exceeded=false;
    std::uint32_t pages=0,bytes=0,read_calls=0,words_checked=0,pointers_checked=0;
    std::uint32_t unread_pages=0,mismatch_count=0,record_count=0;
    std::array<ProfileMismatch,8> mismatches{};
};
// Fixed, copied observations only. No process addresses or file contents.
// Each subsection has its own validity; shared counters are a best-effort
// observation, while settings/appearance/ordered popup use independent seqlocks.
struct LiveDiagnosticSnapshot {
    LiveDiagnosticStatus status=LiveDiagnosticStatus::NoGame;
    bool process_found=false,supported=false,connected=false;
    SharedControl shared{};
    ModuleObservation module_observation{};
    ProfileObservation profile_observation{};
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
    bool emitters_valid=false;
    EmitterDiagnosticsWire emitters{};
    HealthReport health_reference{};
    bool health_last_capture_valid=false;
    HealthCaptureWire health_last_capture{};
};
// NUL-terminated JSON on success, exact byte count excludes NUL. Capacity is
// capped at16384; failures return size0 and an empty string when possible.
bool format_live_diagnostics(const LiveDiagnosticSnapshot&,char* output,
                             std::size_t capacity,std::size_t& size);
}
