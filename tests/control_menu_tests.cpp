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
    assert(ids.size()==unsigned(ControlId::Count));
    bool camera_return=false,free_camera=false,mapping=false;
    for(const auto& spec:control_specs()){
        camera_return|=std::strcmp(spec.label.english,"Camera return speed")==0;
        free_camera|=std::strcmp(spec.label.english,"Free camera 360")==0;
        mapping|=std::strcmp(spec.label.english,"Button mapping")==0;
    }
    assert(camera_return && free_camera && mapping);
    bool graphics=false;
    for(const auto& spec:control_specs())graphics|=std::strcmp(spec.label.english,"Graphics")==0;
    assert(graphics);
    Settings s;
    assert(!change_control(s,ControlId::GraphicsMenu,1));
    assert(std::strstr(graphics_spec(GraphicsId::AntiAliasing).help.english,"Restart")!=nullptr);
    assert(std::strstr(graphics_spec(GraphicsId::AntiAliasing).help.russian,"Перезапуск")!=nullptr);
    for(const auto& spec:graphics_specs()){
        for(auto language:{Language::English,Language::Russian}){
            assert(std::strlen(spec.label.get(language))>0 && std::strlen(spec.help.get(language))>20);
            assert(!graphics_value_text(spec,s).empty());
        }
    }
    assert(s.texture_filtering==TextureFilter::Native && s.anti_aliasing==AntiAliasing::Native
        && s.shadow_mode==ShadowMode::Native && s.draw_distance_scale==1);
    for(unsigned i=0;i<7;++i)assert(change_graphics(s,GraphicsId::TextureFiltering,1));
    assert(s.texture_filtering==TextureFilter::Native);
    assert(change_graphics(s,GraphicsId::TextureFiltering,-1) && s.texture_filtering==TextureFilter::Aniso16);
    for(unsigned i=0;i<7;++i)assert(change_graphics(s,GraphicsId::TextureFiltering,-1));
    assert(s.texture_filtering==TextureFilter::Aniso16);
    for(auto id:{GraphicsId::AntiAliasing,GraphicsId::Shadows}){
        assert(change_graphics(s,id,-1));assert(change_graphics(s,id,-1));
    }
    assert(s.anti_aliasing==AntiAliasing::Native && s.shadow_mode==ShadowMode::Native);
    for(unsigned i=0;i<12;++i)change_graphics(s,GraphicsId::DrawDistance,-1);
    assert(s.draw_distance_scale==0.5f && !change_graphics(s,GraphicsId::DrawDistance,-1));
    for(unsigned i=0;i<12;++i)change_graphics(s,GraphicsId::DrawDistance,1);
    assert(s.draw_distance_scale==2 && !change_graphics(s,GraphicsId::DrawDistance,1));
    s.language=Language::Russian;s.camera_gain=0.4f;s.gyro_speed=0.3f;s.free_camera=true;s.button_map[0]=255;
    s.anti_aliasing=AntiAliasing::HandheldSsaa;s.shadow_mode=ShadowMode::Off;
    assert(!change_graphics(s,GraphicsId::Reset,1) && !change_graphics(s,GraphicsId::DrawDistance,0));
    assert(reset_graphics(s) && !reset_graphics(s));
    assert(s.language==Language::Russian && s.camera_gain==0.4f && s.gyro_speed==0.3f && s.free_camera && s.button_map[0]==255);
    assert(s.texture_filtering==TextureFilter::Native && s.anti_aliasing==AntiAliasing::Native
        && s.shadow_mode==ShadowMode::Native && s.draw_distance_scale==1);
    s={};
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
    assert(s.gyro_speed==0.8f && s.camera_return_speed==1 && !s.free_camera);
    for(unsigned i=0;i<30;++i)change_control(s,ControlId::CameraReturn,-1);
    assert(s.camera_return_speed==0 && !change_control(s,ControlId::CameraReturn,-1));
    for(unsigned i=0;i<30;++i)change_control(s,ControlId::CameraReturn,1);
    assert(s.camera_return_speed==1);
    assert(change_control(s,ControlId::FreeCamera,1) && s.free_camera);
    assert(!change_control(s,ControlId::ButtonMapping,1));
    for(unsigned i=0;i<20;++i){
        for(auto language:{Language::English,Language::Russian}){
            assert(mapping_row_label(i+1,language)==button_name(i));
            assert(std::strlen(mapping_row_help(i+1,language))>20);
            assert(std::strcmp(mapping_target_label(i,language),button_name(i))==0);
        }
        assert(!set_mapping_target(s,i,i));
        assert(set_mapping_target(s,i,20) && s.button_map[i]==255);
        assert(change_mapping(s,i+1,1) && s.button_map[i]==0);
        assert(change_mapping(s,i+1,-1) && s.button_map[i]==255);
    }
    s.remap_enabled=false;s.gyro_speed=0.3f;s.camera_gain=0.4f;
    assert(change_mapping(s,21,1) && s.remap_enabled && s.button_map==identity_button_map());
    assert(s.gyro_speed==0.3f && s.camera_gain==0.4f && s.free_camera);
    assert(!change_mapping(s,21,1) && !set_mapping_target(s,20,0) && !set_mapping_target(s,0,21));
    std::puts("bilingual menu catalogue: unique translated controls, help and no walk-speed item PASS");
}
