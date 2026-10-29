#pragma once
#include "settings.hpp"
#include <array>
#include <string>

namespace re4 {
struct Bilingual {
    const char* english;
    const char* russian;
    const char* get(Language language) const { return language==Language::Russian ? russian : english; }
};
enum class ControlId : unsigned {
    Language, Curve, Analog, Turn, Camera, AimX, AimY, LeftDeadzone, RightDeadzone,
    Gyro, GyroX, GyroY, InvertX, InvertY, Reset, Count
};
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
}
