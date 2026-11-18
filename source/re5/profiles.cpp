#include "re5/profiles.hpp"
#include <string>
namespace re5 {
namespace {
bool valid_domain(re4::ProfileDomain domain){return domain==re4::ProfileDomain::Controls||domain==re4::ProfileDomain::Graphics;}
std::string custom_path(const char* path,re4::ProfileDomain domain,unsigned player){
 return std::string(path)+(domain==re4::ProfileDomain::Graphics?".graphics-custom":player==0?".controls-1-custom":".controls-2-custom");
}
void copy_domain(Settings& destination,const Settings& source,re4::ProfileDomain domain,unsigned player){
 if(domain==re4::ProfileDomain::Graphics)destination.graphics=source.graphics;
 else re4::copy_profile(destination.players[player],source.players[player],domain);
}
}
re4::Settings graphics_view(const Settings& settings){
 auto view=native_controls();view.language=settings.language;
 view.texture_filtering=settings.graphics.filter;view.shadow_mode=settings.graphics.shadows;
 view.anti_aliasing=re4::AntiAliasing::Native;view.draw_distance_scale=settings.graphics.draw_distance;return view;
}
re4::ProfileKind profile_kind(const Settings& settings,re4::ProfileDomain domain,unsigned player){
 if(!valid_domain(domain)||player>=2)return re4::ProfileKind::Custom;
 if(domain==re4::ProfileDomain::Graphics){
  if(same_graphics(settings.graphics,GraphicsOptions{}))return re4::ProfileKind::Default;
  if(same_graphics(settings.graphics,GraphicsOptions{re4::TextureFilter::Aniso16,re4::ShadowMode::Native,AntiAliasing::Smaa,1.75f}))return re4::ProfileKind::Quality;
  return re4::ProfileKind::Custom;
 }
 return re4::profile_kind(settings.players[player],domain);
}
bool save_edited_settings(const char* path,const Settings& settings,re4::ProfileDomain domain,unsigned player){
 if(!path||!*path||!valid_domain(domain)||player>=2||!valid_settings(settings))return false;
 return save_settings_file(custom_path(path,domain,player).c_str(),settings)&&save_settings_file(path,settings);
}
re4::ProfileResult switch_profile(const char* path,Settings& active,re4::ProfileDomain domain,re4::ProfileKind kind,unsigned player){
 using re4::ProfileDomain;using re4::ProfileKind;using re4::ProfileResult;
 if(!path||!*path||!valid_domain(domain)||player>=2||!valid_settings(active)
  ||(kind!=ProfileKind::Default&&kind!=ProfileKind::Custom&&kind!=ProfileKind::RE6&&kind!=ProfileKind::Quality)
  ||(kind==ProfileKind::Quality&&domain!=ProfileDomain::Graphics)
  ||(kind==ProfileKind::RE6&&domain!=ProfileDomain::Controls))return ProfileResult::SaveFailed;
 Settings next=active;
 if(kind==ProfileKind::Custom){
  Settings custom;if(!load_settings_file(custom_path(path,domain,player).c_str(),custom))return ProfileResult::MissingCustom;
  copy_domain(next,custom,domain,player);
 }else if(domain==ProfileDomain::Controls){
  re4::copy_profile(next.players[player],kind==ProfileKind::RE6?re4::re6_profile():native_controls(),domain);
 }else{
  next.graphics=kind==ProfileKind::Quality?
   GraphicsOptions{re4::TextureFilter::Aniso16,re4::ShadowMode::Native,AntiAliasing::Smaa,1.75f}:GraphicsOptions{};
 }
 const auto before=domain==ProfileDomain::Graphics?graphics_view(active):active.players[player];
 const auto after=domain==ProfileDomain::Graphics?graphics_view(next):next.players[player];
 if(domain==ProfileDomain::Graphics?same_graphics(active.graphics,next.graphics):re4::same_profile(before,after,domain))return ProfileResult::Unchanged;
 if(!save_settings_file(path,next))return ProfileResult::SaveFailed;
 active=next;return ProfileResult::Applied;
}
}
