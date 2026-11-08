#pragma once
#include <array>
#include <cstdint>
namespace re4 {
struct LiveHudMatrix { bool valid=false; std::array<float,16> values{}; };
// x,y,width,height,near,far in the native software-context coordinate space.
struct LiveHudViewport { bool valid=false; std::array<float,6> values{}; };
// Best-effort cached state. Never ordered with the popup command and never a
// screen visibility measurement. No process addresses leave the reader.
struct LiveHudRendererObservation {
    bool context_valid=false,context_stable=false;
    std::uint32_t context_index=0;
    bool ordered=false;
    LiveHudViewport logical_viewport{},adjusted_viewport{};
    LiveHudMatrix model{},view{},projection{},correction{},combined{};
};
}
