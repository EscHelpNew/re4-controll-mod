#pragma once
#include "settings.hpp"
#include "profiles.hpp"
#include <array>
#include <string>

namespace re4 {
struct Bilingual {
    const char* english;
    const char* russian;
    const char* get(Language language) const { return language==Language::Russian ? russian : english; }
};
enum class ControlId : unsigned {
    Language, Curve, Analog, Turn, Camera, CameraReturn, FreeCamera, ButtonMapping, Profile, AimX, AimY, RightAimX, RightAimY, LeftDeadzone, RightDeadzone,
    Gyro, GyroSpeed, GyroDeadzone, GyroX, GyroY, InvertX, InvertY, FirstPerson, Reticle, ReticleSize, MoveAiming, MoveKnife, ThirdPersonDistance, UnifiedControls, Reset, Count
};
enum class ControlGroup : unsigned { Sticks, Aiming, Gyro, Camera, Reticle, Mapping, Profiles, Count };
struct ControlGroupSpec {
    ControlGroup id;
    Bilingual label,help;
};
const std::array<ControlGroupSpec,unsigned(ControlGroup::Count)>& control_groups();
const ControlGroupSpec& control_group_spec(ControlGroup);
unsigned control_group_count(ControlGroup);
ControlId control_group_item(ControlGroup,unsigned index);
enum class ControlKind { Selector, Toggle, Slider, Action };
struct ControlSpec {
    ControlId id{};
    ControlKind kind{};
    Bilingual label{},help{};
    float Settings::*value=nullptr;
    bool Settings::*toggle=nullptr;
    float step=0,minimum=0,maximum=0;
};
const std::array<ControlSpec,unsigned(ControlId::Count)>& control_specs();
const ControlSpec& control_spec(ControlId id);
const char* curve_name(StickCurve curve,Language language);
std::string control_value_text(const ControlSpec&,const Settings&);
bool change_control(Settings&,ControlId,int direction);
enum class GraphicsId : unsigned { TextureFiltering, AntiAliasing, DrawDistance, Shadows, Profile, Reset, Count };
struct GraphicsSpec {
    GraphicsId id{};
    ControlKind kind{};
    Bilingual label{},help{};
};
const std::array<GraphicsSpec,unsigned(GraphicsId::Count)>& graphics_specs();
const GraphicsSpec& graphics_spec(GraphicsId);
std::string graphics_value_text(const GraphicsSpec&,const Settings&);
bool change_graphics(Settings&,GraphicsId,int direction);
bool reset_graphics(Settings&);
const char* profile_name(ProfileKind,Language);
inline constexpr std::array<ProfileKind,3> kControlProfiles{{ProfileKind::Default,ProfileKind::RE9,ProfileKind::Custom}};
const char* control_profile_help(ProfileKind,Language);
const char* profile_result_text(ProfileResult,Language);
constexpr unsigned kMappingRowCount=kButtonCount+2;
std::string mapping_row_label(unsigned,Language);
std::string mapping_row_value(unsigned,const Settings&);
const char* mapping_row_help(unsigned,Language);
const char* mapping_target_label(unsigned,Language);
const char* mapping_target_help(Language);
unsigned mapping_target_index(const Settings&,unsigned source);
bool set_mapping_target(Settings&,unsigned source,unsigned target);
bool change_mapping(Settings&,unsigned row,int direction);
}
