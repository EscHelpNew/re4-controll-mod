#pragma once
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <utility>
using u64 = std::uint64_t;
using Result = unsigned;
#define R_FAILED(v) ((v) != 0)
#define R_SUCCEEDED(v) ((v) == 0)
#define NORETURN [[noreturn]]
#define EXL_ABORT(...) std::abort()
constexpr unsigned InfoType_ProgramId=1, CUR_PROCESS_HANDLE=0;
struct MemoryInfo {std::uintptr_t addr=0;std::size_t size=0;unsigned perm=0;};
namespace game_test {
inline u64 program=0x010099A00BC1E000, tick=1;
inline unsigned installed=0, initialized=0, mounts=0, fs_opens=0;
inline unsigned mount_result=1, mount_failures=0, fs_reads=0, fs_closes=0;
inline std::vector<std::uint8_t> settings_primary,settings_backup;
inline unsigned sensor_starts=0, sensor_stops=0, sensor_reads=0, start_result=0;
inline unsigned pad_attributes=1, native_aim=1;
inline std::uint64_t pad_buttons=0;
inline float rendered_pitch=0,rendered_camera_yaw=0;
inline unsigned camera_gate_change=0;
inline int operation_mode=0;
inline float motion_speed_seen=0;
inline float aim_step=0.08f, sway_baseline=0;
inline bool apply_sway=false;
inline int camera_x=0, camera_y=0;
inline unsigned directions_seen=0;
inline std::uintptr_t base=0;
struct Range { std::uintptr_t m_Start=0; std::size_t m_Size=0; };
struct Main { Range m_Total; };
inline Main module;
inline std::vector<MemoryInfo> memory_maps;
}
inline Result svcQueryMemory(MemoryInfo* out,std::uint32_t* page,std::uintptr_t address){
    for(const auto& region:game_test::memory_maps)if(address>=region.addr && address-region.addr<region.size){
        *out=region;*page=0;return 0;
    }
    return 1;
}
inline Result svcGetInfo(u64* out, unsigned, unsigned, unsigned) { *out=game_test::program; return 0; }
inline u64 svcGetSystemTick() { return game_test::tick; }
namespace exl::util { inline const auto& GetMainModuleInfo() { return game_test::module; } }
namespace exl::hook { inline void Initialize() { ++game_test::initialized; } }
template<auto Function> struct TestHook {
    // Match exlaunch's non-constexpr forwarding function, including in C++17.
    // A constexpr function-pointer stand-in changes lambda constexpr analysis.
    template<class... Args> static decltype(auto) Orig(Args&&... args){
        return Function(std::forward<Args>(args)...);
    }
    static void InstallAtOffset(std::size_t) { ++game_test::installed; }
    template<class T> static void InstallAtFuncPtr(T) { ++game_test::installed; }
};
#define HOOK_DEFINE_TRAMPOLINE(Name) struct Name : TestHook<&test_##Name>
