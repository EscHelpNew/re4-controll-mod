#include "re4/control_menu.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <set>
int main(){
    using namespace re4;
    std::set<unsigned> ids;
    for(const auto& spec:control_specs()){
        assert(spec.label.english && spec.label.russian && spec.help.english && spec.help.russian);
        assert(std::strlen(spec.label.english)>0 && std::strlen(spec.help.russian)>0);
        assert(ids.insert(unsigned(spec.id)).second);
        assert(std::strstr(spec.label.english,"Walk")==nullptr);
        assert(std::strstr(spec.label.russian,"ходьбы")==nullptr);
    }
    assert(ids.size()==19);
    Settings s;
    assert(change_control(s,ControlId::Language,1) && s.language==Language::Russian);
    assert(control_value_text(control_spec(ControlId::Language),s)=="Русский");
    for(unsigned i=0;i<4;++i)assert(change_control(s,ControlId::Curve,1));
    assert(s.stick_curve==StickCurve::Linear);
    assert(change_control(s,ControlId::Curve,-1) && s.stick_curve==StickCurve::Hybrid);
    for(unsigned i=0;i<30;++i)change_control(s,ControlId::Turn,1);
    assert(s.turn_gain==1);
    assert(!change_control(s,static_cast<ControlId>(999),1));
    change_control(s,ControlId::Gyro,1);assert(s.gyro_enabled);
    assert(change_control(s,ControlId::Reset,1));
    assert(s.language==Language::English && s.left_deadzone==0.05f && s.invert_x && !s.gyro_enabled);
    std::puts("bilingual menu catalogue: unique translated controls, help and no walk-speed item PASS");
}
