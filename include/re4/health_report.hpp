#pragma once
#include <cstdint>
namespace re4 {
enum class HealthReportStatus : std::uint32_t {ModuleMissing,NoGame,UnsupportedBuild,ReadFailed,NoTarget,Unstable,Collected};
enum class HealthReportGate : std::uint32_t {None,SettingsRead,SettingsPending,BarsOff,NumbersOff,Context,Pool,Target,Health,Weapon,ModeRank,Class,Subtype,BodyStatus,Coefficient,Prediction,Unstable,UnsupportedFamily};
inline constexpr std::uint32_t health_report_gate_bit(HealthReportGate gate){return gate==HealthReportGate::None?0:1u<<unsigned(gate);}
// Diagnostic schema1 evaluates the version18 predictor, not a captured HUD unit.
// first_failed_gate orders UI/settings, context, then native eligibility checks.
// Unstable/NoTarget are observations, not proof of an atomic game snapshot.
// No process addresses are exported. RVAs use UINT32_MAX when outside main/unread.
struct HealthReport {
    HealthReportStatus status=HealthReportStatus::ModuleMissing;
    HealthReportGate first_failed_gate=HealthReportGate::None;
    std::uint32_t schema=1,predictor_version=18,ordinary_scope=1,read_mask=0,rejected_mask=0,ipc_reads=0;
    std::uint32_t settings_sequence=0,applied_sequence=0,settings_source=0;
    bool settings_valid=false,settings_applied=false,bars_on=false,numbers_on=false,observed_consistent=false;
    std::uint32_t routine_pack=0,controller_pack=0,input_held_low=0;
    std::uint32_t pool_count=0,pool_stride=0,target_slot=UINT32_MAX,flags=0;
    std::uint32_t vptr_rva=UINT32_MAX,update_rva=UINT32_MAX,table_rva=UINT32_MAX;
    bool vptr_in_main=false,update_in_main=false,link944=false,link974=false;
    std::uint8_t type=0,subtype=0,part_flags=0,body_status=0,phase=0,mode=0,rank=0,weapon_index=0;
    std::int16_t current=0,maximum=0;
    std::int32_t coefficient=0;
    std::uint32_t predicted_unit=0;
};
enum HealthReportRead : std::uint32_t {HealthSettings=1,HealthContext=2,HealthPool=4,HealthIdentity=8,HealthValues=16,HealthBody=32,HealthGlobals=64,HealthCoefficient=128};
}
