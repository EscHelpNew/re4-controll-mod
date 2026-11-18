#include "re4/live_diagnostics.hpp"
#include "re4/character_loading.hpp"
#include "re4/version.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>

namespace re4 {
namespace {
class Json {
    char* out_;std::size_t cap_,at_=0;bool ok_=true;
    bool first_[8]{};unsigned depth_=0;
public:
    Json(char* out,std::size_t cap):out_(out),cap_(cap){}
    void raw(const char* text,std::size_t size){
        if(!ok_)return;
        if(size>=cap_-at_){ok_=false;return;}
        std::memcpy(out_+at_,text,size);at_+=size;
    }
    void raw(const char* text){raw(text,std::strlen(text));}
    void string(const char* text,std::size_t limit){
        raw("\"");constexpr char hex[]="0123456789abcdef";
        for(std::size_t i=0;i<limit&&text[i];++i){
            const auto c=static_cast<unsigned char>(text[i]);
            if(c=='"'||c=='\\'){const char escape[]{'\\',char(c)};raw(escape,2);}
            else if(c<32||c>=127){const char escape[]{'\\','u','0','0',hex[c>>4],hex[c&15]};raw(escape,6);}
            else raw(text+i,1);
        }
        raw("\"");
    }
    void key(const char* text){
        if(!depth_){ok_=false;return;}
        if(!first_[depth_-1])raw(",");
        first_[depth_-1]=false;
        string(text,std::strlen(text));raw(":");
    }
    void array(){if(depth_==8){ok_=false;return;}raw("[");first_[depth_++]=true;}
    void element(){if(!depth_){ok_=false;return;}if(!first_[depth_-1])raw(",");first_[depth_-1]=false;}
    void end_array(){if(!depth_){ok_=false;return;}--depth_;raw("]");}
    void object(){if(depth_==8){ok_=false;return;}raw("{");first_[depth_++]=true;}
    void end(){if(!depth_){ok_=false;return;}--depth_;raw("}");}
    void number(std::uint64_t value){char text[32];const auto r=std::to_chars(text,text+sizeof(text),value);raw(text,std::size_t(r.ptr-text));}
    void signed_number(std::int64_t value){char text[32];const auto r=std::to_chars(text,text+sizeof(text),value);raw(text,std::size_t(r.ptr-text));}
    void real(float value){
        if(!std::isfinite(value)){raw("null");return;}
        char text[48];const auto r=std::to_chars(text,text+sizeof(text),value,std::chars_format::general,9);
        if(r.ec!=std::errc{}){ok_=false;return;}raw(text,std::size_t(r.ptr-text));
    }
    void u(const char* name,std::uint64_t value){key(name);number(value);}
    void i(const char* name,std::int64_t value){key(name);signed_number(value);}
    void b(const char* name,bool value){key(name);raw(value?"true":"false");}
    void f(const char* name,float value){key(name);real(value);}
    void s(const char* name,const char* text,std::size_t limit=256){key(name);string(text,limit);}
    void null(const char* name){key(name);raw("null");}
    template<class T>void integers(const char* name,const T* values,std::size_t count){
        key(name);raw("[");for(std::size_t i=0;i<count;++i){if(i)raw(",");signed_number(values[i]);}raw("]");
    }
    void floats(const char* name,const float* values,std::size_t count){
        key(name);raw("[");for(std::size_t i=0;i<count;++i){if(i)raw(",");real(values[i]);}raw("]");
    }
    bool finish(std::size_t& size){if(!ok_||depth_){out_[0]=0;size=0;return false;}out_[at_]=0;size=at_;return true;}
};
const char* status_name(LiveDiagnosticStatus status){
    switch(status){
        case LiveDiagnosticStatus::NoGame:return "no_game";
        case LiveDiagnosticStatus::ServiceUnavailable:return "service_unavailable";
        case LiveDiagnosticStatus::UnsupportedBuild:return "unsupported_build";
        case LiveDiagnosticStatus::ModuleMissing:return "module_missing";
        case LiveDiagnosticStatus::DiscoveryLimited:return "discovery_limited";
        case LiveDiagnosticStatus::ReadFailed:return "read_failed";
        case LiveDiagnosticStatus::Connected:return "connected";
    }
    return "unknown";
}
const char* reason_name(unsigned reason){
    constexpr const char* errors[]{"none","mount_unavailable","file_open","file_size","file_read","asset_mismatch",
        "unsupported_rig","native_initialization","renderer_unavailable","skinning_failed","texture_unavailable","renderer_changed"};
    constexpr const char* pending[]{"waiting_renderer","waiting_player","waiting_scene","waiting_state","waiting_model",
        "waiting_bones","waiting_chain","waiting_alternate","waiting_material","waiting_effect","waiting_character",
        "waiting_mount","waiting_retirement","reading_asset","waiting_pack_command","waiting_pack_busy","waiting_pack_io",
        "waiting_probe_fence","waiting_upload_fence","waiting_reservations","waiting_skin","waiting_draw","waiting_hands",
        "waiting_frame_slot","waiting_render_lock","waiting_native_skin","waiting_slot_consumer","waiting_bin_bones"};
    if(reason<sizeof(errors)/sizeof(*errors))return errors[reason];
    if(reason>=100&&reason<100+sizeof(pending)/sizeof(*pending))return pending[reason-100];
    return "unknown";
}
const char* loading_name(unsigned stage){
    constexpr const char* names[]{"none","context","mapping","registry_capacity","resident_entry","io_state","thread","prefix",
        "compressed_variant","file_open","file_size","allocation","heap","headroom","revalidation","handshake","native_io",
        "blob","metadata","descriptor_index","descriptor_owner","descriptor_capacity","creator","unique_count"};
    return stage<sizeof(names)/sizeof(*names)?names[stage]:"unknown";
}
void settings_json(Json& j,const Settings& s){
#define B(name) j.b(#name,s.name)
#define F(name) j.f(#name,s.name)
#define E(name) j.u(#name,unsigned(s.name))
    E(language);E(stick_curve);B(analog_enabled);B(gyro_enabled);B(invert_x);B(invert_y);j.u("calibration_request",s.calibration_request);
    F(turn_gain);F(camera_gain);F(aim_gain_x);F(aim_gain_y);F(right_aim_gain_x);F(right_aim_gain_y);
    F(left_deadzone);F(right_deadzone);F(gyro_gain_x);F(gyro_gain_y);F(gyro_speed);F(gyro_deadzone_dps);F(gyro_bias_x);F(gyro_bias_y);
    F(movement_gain);B(remap_enabled);j.integers("button_map",s.button_map.data(),s.button_map.size());
    B(free_camera);F(camera_return_speed);F(third_person_distance);E(texture_filtering);E(shadow_mode);E(anti_aliasing);F(draw_distance_scale);
    B(first_person);B(move_while_aiming);B(move_while_knife);B(move_while_reloading);B(unified_controls);B(re6_response);E(reticle);E(reticle_size);
    B(enemy_health_bars);B(enemy_health_numbers);B(grenade_landing_spot);B(skip_pickup_confirmation);B(skip_startup_logos);
    B(disable_aim_sway);B(dpad_weapon_switching);B(extended_weapon_upgrade);B(case_ammo_counter);E(character_appearance);
#undef B
#undef F
#undef E
}
}
namespace {
void knife_part_json(Json& j,const char* name,const LiveKnifePart& p){
    j.key(name);j.object();j.b("valid",p.valid);j.b("present",p.present);
    j.b("visible",p.visible);j.b("next_present",p.next_present);
    j.b("bin_readable",p.bin_readable);j.b("tpl_readable",p.tpl_readable);
    j.b("weapon_bin",p.weapon_bin);j.b("weapon_tpl",p.weapon_tpl);
    j.b("fields_readable",p.fields_readable);j.b("vtable_in_main",p.vtable_in_main);
    if(p.fields_readable&&p.vtable_in_main)j.u("vtable_offset",p.vtable_offset);else j.null("vtable_offset");
    j.b("matrix_finite",p.fields_readable&&p.matrix_finite);
    if(p.fields_readable)j.u("animation",p.animation);else j.null("animation");
    if(p.valid){j.u("flags",p.flags);j.u("alpha",p.alpha);}else{j.null("flags");j.null("alpha");}
    if(p.bin_readable)j.u("bin_bones",p.bin_bones);else j.null("bin_bones");
    if(p.bin_readable){j.u("bin_flags",p.bin_flags);j.u("bin_header_3b",p.bin_header_3b);
        j.u("bin_vertices",p.bin_vertices);j.u("bin_normals",p.bin_normals);}
    else{j.null("bin_flags");j.null("bin_header_3b");j.null("bin_vertices");j.null("bin_normals");}
    j.i("bin_entry",p.bin_entry);j.i("tpl_entry",p.tpl_entry);j.end();
}
void knife_held_json(Json& j,const char* name,const LiveKnifeHeld& h){
    j.key(name);j.object();j.b("valid",h.valid);j.b("present",h.present);j.b("stable",h.stable);
    j.b("aliases_other",h.aliases_other);j.b("vtable_in_main",h.vtable_in_main);
    if(h.valid&&h.vtable_in_main)j.u("vtable_offset",h.vtable_offset);else j.null("vtable_offset");
    j.b("native_knife_class",h.native_knife_class);j.b("parent_is_player",h.parent_is_player);
    j.b("actor_visible",h.actor_visible);j.b("control_hidden",h.control_hidden);
    if(h.valid){j.u("flags",h.flags);j.u("type",h.type);j.u("bones",h.bones);j.u("control",h.control);}
    else{j.null("flags");j.null("type");j.null("bones");j.null("control");}
    j.b("root_present",h.root_present);j.b("root_valid",h.root_valid);
    j.b("root_parent_is_grip",h.root_parent_is_grip);j.b("bin_bones_match",h.bin_bones_match);
    j.b("root_matrix_valid",h.root_matrix_valid);
    if(h.root_matrix_valid)j.floats("root_matrix",h.root_matrix.data(),h.root_matrix.size());else j.null("root_matrix");
    knife_part_json(j,"part",h.part);j.end();
}
void knife_json(Json& j,const LiveKnifeObservation& k){
    j.key("knife");j.object();j.b("valid",k.valid);j.b("stable",k.stable);
    j.u("read_calls",k.read_calls);j.u("read_bytes",k.read_bytes);j.b("budget_exhausted",k.budget_exhausted);
    j.b("player_valid",k.player_valid);j.b("player_stable",k.player_stable);j.b("vtable_in_main",k.vtable_in_main);
    if(k.player_valid&&k.vtable_in_main)j.u("player_vtable_offset",k.player_vtable_offset);else j.null("player_vtable_offset");
    if(k.player_valid){j.u("player_flags",k.player_flags);j.u("player_type",k.player_type);j.u("player_bones",k.player_bones);
        j.integers("routine",k.routine.data(),k.routine.size());}
    else{j.null("player_flags");j.null("player_type");j.null("player_bones");j.null("routine");}
    j.b("state_valid",k.state_valid);j.b("asset_matches_state70",k.asset_matches_state70);j.b("grip_valid",k.grip_valid);
    if(k.state_valid){j.u("native_character",k.native_character);j.u("weapon_id",k.weapon_id);j.u("weapon_sub",k.weapon_sub);}
    else{j.null("native_character");j.null("weapon_id");j.null("weapon_sub");}
    const auto& b=k.body;j.key("body");j.object();j.b("valid",b.valid);j.b("stable",b.stable);
    j.b("face_present",b.face_present);j.b("right_hand_present",b.right_hand_present);j.b("left_hand_present",b.left_hand_present);
    j.b("aliases_right_hand",b.aliases_right_hand);j.b("aliases_left_hand",b.aliases_left_hand);
    j.b("chain_valid",b.chain_valid);j.b("chain_cycle",b.chain_cycle);j.b("chain_truncated",b.chain_truncated);
    j.b("face58_member",b.face58_member);j.u("chain_count",b.chain_count);j.i("face58_index",b.face58_index);
    j.b("directory_valid",b.directory_valid);j.b("directory_complete",b.directory_complete);
    j.b("source_shape_matches",b.source_shape_matches);j.b("source_shape_metadata_only",true);
    const auto& buffer=b.buffers;j.key("buffers");j.object();
    j.b("slot_readable",buffer.slot_readable);j.b("slot_valid",buffer.slot_valid);j.b("stable",buffer.stable);
    if(buffer.slot_readable)j.u("slot",buffer.slot);else j.null("slot");
    j.b("samples_only",true);j.b("positions_present",buffer.positions_present);
    j.b("positions_sample_readable",buffer.positions_readable);j.b("positions_sample_finite",buffer.positions_finite);
    j.b("normals_present",buffer.normals_present);j.b("normals_sample_readable",buffer.normals_readable);
    j.b("normals_sample_finite",buffer.normals_finite);j.end();
    knife_part_json(j,"face58",b.face58);j.end();
    knife_held_json(j,"held_primary",k.held[0]);knife_held_json(j,"held_secondary",k.held[1]);j.end();
}
template<class T>void hud_values_json(Json& j,const char* name,const T& field,bool context){
    if(context&&field.valid)j.floats(name,field.values.data(),field.values.size());else j.null(name);
}
void hud_cached_json(Json& j,const LiveHudRendererObservation& h){
    const bool valid=h.context_valid&&h.context_stable;
    j.key("hud_cached");j.object();j.b("valid",valid);j.b("ordered_with_popup",false);j.b("best_effort",true);
    j.b("context_valid",h.context_valid);j.b("context_stable",h.context_stable);j.u("context_index",h.context_index);
    j.s("matrix_layout","column_major");
    hud_values_json(j,"logical_viewport",h.logical_viewport,valid);hud_values_json(j,"adjusted_viewport",h.adjusted_viewport,valid);
    hud_values_json(j,"model",h.model,valid);hud_values_json(j,"view",h.view,valid);
    hud_values_json(j,"projection",h.projection,valid);hud_values_json(j,"correction",h.correction,valid);
    hud_values_json(j,"combined",h.combined,valid);j.end();
}
void module_observation_json(Json& j,const LiveDiagnosticSnapshot& snapshot){
    const auto& o=snapshot.module_observation;
    if((!o.found&&!o.scan_reads&&!o.scan_limit)||!snapshot.supported){j.null("module_observation");return;}
    j.key("module_observation");j.object();j.u("schema",1);j.b("found",o.found);j.b("stable",o.stable);
    j.b("accepted_as_connection",snapshot.connected&&o.status==1&&o.stable);if(o.found){j.s("version",o.version.data(),o.version.size());j.u("abi",o.abi);j.u("status",o.status);j.u("capabilities",o.capabilities);j.u("frames",o.frames);}
    else {j.null("version");j.null("abi");j.null("status");j.null("capabilities");j.null("frames");}
    j.u("scan_reads",o.scan_reads);j.u("scan_bytes",o.scan_bytes);j.b("scan_limit",o.scan_limit);j.end();
}
void profile_observation_json(Json& j,const LiveDiagnosticSnapshot& snapshot){
    const auto& o=snapshot.profile_observation;
    if(!o.attempted||!snapshot.supported){j.null("profile_observation");return;}
    j.key("profile_observation");j.object();j.u("schema",1);j.s("scope","legacy_v18_extra_requirements");
    j.b("diagnostic_only",true);j.b("current_profile_gate",false);j.b("best_effort",true);
    j.b("process_stable",o.stable);j.b("complete",o.complete);j.b("budget_exceeded",o.budget_exceeded);
    j.u("pages",o.pages);j.u("bytes",o.bytes);j.u("read_calls",o.read_calls);
    j.u("words_checked",o.words_checked);j.u("pointers_checked",o.pointers_checked);
    j.u("unread_pages",o.unread_pages);j.u("mismatch_count",o.mismatch_count);
    j.key("mismatches");j.array();
    for(unsigned i=0;i<std::min<unsigned>(o.record_count,o.mismatches.size());++i){
        const auto& m=o.mismatches[i];j.element();j.object();j.s("kind",m.pointer?"relative_pointer":"word");
        j.u("rva",m.rva);j.b("in_bounds",m.in_bounds);j.b("readable",m.readable);
        if(m.pointer){j.u("expected_rva",m.expected);j.b("observed_in_main",m.observed_in_main);j.b("observed_null",m.observed_null);
            if(m.readable&&m.observed_in_main)j.u("observed_rva",m.observed);else j.null("observed_rva");}
        else {j.u("expected_word",m.expected);if(m.readable)j.u("observed_word",m.observed);else j.null("observed_word");}
        j.end();
    }
    j.end_array();j.end();
}
void health_reference_json(Json& j,const HealthReport& h){
    constexpr const char* statuses[]={"module_missing","no_game","unsupported_build","read_failed","no_target","unstable","collected"};
    constexpr const char* gates[]={"none","settings_read","settings_pending","bars_off","numbers_off","context","pool","target","health",
        "weapon","mode_rank","class","subtype","body_status","coefficient","prediction","unstable","unsupported_family"};
    j.key("health_reference");j.object();j.u("schema",h.schema);j.u("predictor_version",h.predictor_version);
    j.s("scope","ordinary_body_reference");j.b("captured_hud_unit",false);j.b("best_effort",true);
    j.u("status",unsigned(h.status));j.s("status_name",unsigned(h.status)<7?statuses[unsigned(h.status)]:"unknown");
    j.u("first_failed_gate",unsigned(h.first_failed_gate));j.s("gate_name",unsigned(h.first_failed_gate)<18?gates[unsigned(h.first_failed_gate)]:"unknown");
    j.u("read_mask",h.read_mask);j.u("rejected_mask",h.rejected_mask);j.u("ipc_reads",h.ipc_reads);j.b("observed_consistent",h.observed_consistent);
    j.b("settings_valid",h.settings_valid);j.b("settings_applied",h.settings_applied);j.b("bars_on",h.bars_on);j.b("numbers_on",h.numbers_on);
    j.u("settings_sequence",h.settings_sequence);j.u("applied_sequence",h.applied_sequence);j.u("settings_source",h.settings_source);
    j.u("routine_pack",h.routine_pack);j.u("controller_pack",h.controller_pack);j.u("input_held_low",h.input_held_low);
    j.u("pool_count",h.pool_count);j.u("pool_stride",h.pool_stride);
    if(h.target_slot!=UINT32_MAX)j.u("target_slot",h.target_slot);else j.null("target_slot");
    j.u("flags",h.flags);j.u("type",h.type);j.u("subtype",h.subtype);j.i("current",h.current);j.i("maximum",h.maximum);
    j.u("part_flags",h.part_flags);j.u("body_status",h.body_status);j.u("phase",h.phase);j.b("link944",h.link944);j.b("link974",h.link974);
    j.u("mode",h.mode);j.u("rank",h.rank);j.u("weapon_index",h.weapon_index);
    if(h.vptr_in_main&&h.vptr_rva!=UINT32_MAX)j.u("vptr_rva",h.vptr_rva);else j.null("vptr_rva");
    if(h.update_in_main&&h.update_rva!=UINT32_MAX)j.u("update_rva",h.update_rva);else j.null("update_rva");
    if(h.table_rva!=UINT32_MAX)j.u("table_rva",h.table_rva);else j.null("table_rva");
    j.i("coefficient",h.coefficient);j.u("predicted_unit",h.predicted_unit);j.end();
}
void health_last_capture_json(Json& j,const LiveDiagnosticSnapshot& snapshot){
    const auto& c=snapshot.health_last_capture;
    if(!snapshot.health_last_capture_valid||!health_capture_header_valid(c)||(c.sequence&1)||c.valid!=1){
        j.null("health_last_capture");return;
    }
    using F=HealthCaptureField;
    const auto value=[&](F field){return c.words[unsigned(field)];};
    const auto wide=[&](F low,F high){return std::uint64_t(value(low))|(std::uint64_t(value(high))<<32);};
    const auto observed=[&](F field){return (value(F::ReadMask)&(std::uint32_t(1)<<unsigned(field)))!=0;};
    j.key("health_last_capture");j.object();j.u("schema",c.version);j.u("sequence",c.sequence);
    j.b("retained",true);j.b("provider_observation",true);
    j.u("capture_view_frame",wide(F::ViewFrameLo,F::ViewFrameHi));
    const auto captured=wide(F::ControlFrameLo,F::ControlFrameHi),current=snapshot.shared.frames;
    j.u("capture_control_frame",captured);j.u("current_module_frame",current);
    const bool comparable=observed(F::ControlFrameLo)&&observed(F::ControlFrameHi)&&current>=captured;
    j.b("age_comparable",comparable);if(comparable)j.u("age_frames",current-captured);else j.null("age_frames");
    // Opaque identity stays split into exact unsigned words; it is never an address.
    j.u("target_key_lo",value(F::TargetKeyLo));j.u("target_key_hi",value(F::TargetKeyHi));
    const auto hp=value(F::Health);j.i("current",std::int16_t(hp));j.i("maximum",std::int16_t(hp>>16));
    j.u("weapon_stamp_before",value(F::WeaponBefore));j.u("weapon_stamp_after",value(F::WeaponAfter));
    j.u("computed_unit",value(F::ComputedUnit));j.u("published_unit",value(F::PublishedUnit));
    constexpr const char* gates[]={"none","settings_read","settings_pending","bars_off","numbers_off","context","pool","target","health",
        "weapon","mode_rank","class","subtype","body_status","coefficient","prediction","unstable","unsupported_family"};
    const auto gate=value(F::FailedGate);j.u("first_failed_gate",gate);j.s("gate_name",gate<18?gates[gate]:"unknown");
    const auto outcome=value(F::Outcome);j.u("outcome",outcome);j.s("outcome_name",outcome==1?"published":outcome==2?"stamp_mismatch":"unknown");
    constexpr F fields[]={F::Flags,F::Type,F::Subtype,F::BodyStatus,F::Phase,F::Mode,F::Rank,F::WeaponIndex};
    constexpr const char* names[]={"flags","type","subtype","body_status","phase","mode","rank","weapon_index"};
    for(unsigned i=0;i<8;++i)j.u(names[i],value(fields[i]));
    for(const auto field:{F::VptrRva,F::UpdateRva,F::TableRva}){
        const char* name=field==F::VptrRva?"vptr_rva":field==F::UpdateRva?"update_rva":"table_rva";
        if(observed(field)&&value(field)!=UINT32_MAX)j.u(name,value(field));else j.null(name);
    }
    j.i("coefficient",std::int32_t(value(F::Coefficient)));j.u("read_mask",value(F::ReadMask));
    j.u("link_mask",value(F::LinkMask));j.b("link944",(value(F::LinkMask)&1)!=0);j.b("link974",(value(F::LinkMask)&2)!=0);
    j.u("pool_count",value(F::PoolCount));j.u("pool_stride",value(F::PoolStride));
    if(observed(F::GuardRva)&&observed(F::GuardExpected)&&observed(F::GuardObserved)&&value(F::GuardRva)!=UINT32_MAX){
        j.key("native_guard");j.object();j.u("rva",value(F::GuardRva));j.u("expected",value(F::GuardExpected));j.u("observed",value(F::GuardObserved));j.end();
    }else j.null("native_guard");
    j.end();
}
void emitters_json(Json& j,const LiveDiagnosticSnapshot& snapshot){
    const auto& wire=snapshot.emitters;
    if(!snapshot.emitters_valid||!emitter_diagnostics_header_valid(wire)||(wire.sequence&1)){
        j.null("emitters");return;
    }
    j.key("emitters");j.object();j.u("version",wire.version);j.u("sequence",wire.sequence);
    constexpr const char* channels[]{"laser","body_hand","held","emitter"};
    constexpr const char* fields[]{"attempts","applied","last_lr","failure_lr","failure_code","owner_stage",
        "mismatch_offset","captured_mask","drawn_mask","parent","slot","part"};
    for(unsigned channel=0;channel<kEmitterDiagnosticsChannels;++channel){
        const auto* record=wire.records[channel];j.key(channels[channel]);j.object();
        for(unsigned field=0;field<12;++field)j.u(fields[field],record[field]);
        float visual[3]{},fresh[3]{};
        std::memcpy(visual,record+12,sizeof(visual));std::memcpy(fresh,record+15,sizeof(fresh));
        j.floats("visual_delta",visual,3);j.floats("fresh_delta",fresh,3);
        j.u("last_result",record[18]);j.u("applied_lr",record[19]);j.end();
    }
    j.end();
}
}
bool format_live_diagnostics(const LiveDiagnosticSnapshot& snapshot,char* output,std::size_t capacity,std::size_t& size){
    size=0;if(!output||!capacity)return false;output[0]=0;
    Json j(output,std::min(capacity,live_diagnostic_json_limit));const auto& c=snapshot.shared;
    j.object();j.u("schema",1);j.s("game","RE4");j.s("overlay_version",kVersion);
    j.key("connection");j.object();j.s("status",status_name(snapshot.status));j.b("process_found",snapshot.process_found);
    j.b("supported",snapshot.supported);j.b("connected",snapshot.connected);j.end();
    module_observation_json(j,snapshot);profile_observation_json(j,snapshot);
    if(!snapshot.connected||!snapshot.supported){j.null("module_version");j.null("module");j.null("health_reference");j.null("health_last_capture");j.end();return j.finish(size);}
    j.s("module_version",c.module_version,sizeof(c.module_version));j.key("module");j.object();
#define U(name) j.u(#name,c.name)
#define F(name) j.f(#name,c.name)
#define I(name) j.i(#name,c.name)
    U(abi);U(status);U(capabilities);U(frames);U(sequence);U(applied_sequence);j.end();
    emitters_json(j,snapshot);
    health_reference_json(j,snapshot.health_reference);
    health_last_capture_json(j,snapshot);
    j.key("controls");j.object();F(raw_lx);F(raw_ly);F(raw_rx);F(raw_ry);F(applied_turn_gain);
    U(direction_calls);U(speed_calls);U(pad_samples);U(last_pad_id);F(maximum_raw_lx);F(last_turn_factor);F(original_yaw_delta);F(corrected_yaw_delta);
    U(movement_calls);U(camera_calls);U(aim_calls);F(movement_factor);U(aim_source);I(applied_aim_x);I(applied_aim_y);F(aim_full_scale);
    U(sensor_result);U(sensor_running);U(sensor_samples);U(sensor_fresh_samples);U(sensor_restarts);U(gyro_active);U(pad_style);
    F(gyro_yaw);F(gyro_pitch);F(gyro_rate_yaw);F(gyro_rate_pitch);U(fps_overrides);U(reticle_draws);U(modern_move_steps);U(modern_pose_steps);U(modern_move_status);F(modern_move_distance);j.end();
    j.key("render");j.object();U(graphics_ready);U(graphics_sampler_calls);U(graphics_sampler_overrides);U(graphics_filter_latched);U(graphics_aa_latched);
    U(graphics_scene_submits);U(graphics_scene_draws);U(graphics_cull_updates);F(graphics_native_far);F(graphics_effective_far);
    U(graphics_resolution_requests);I(graphics_display_index);I(graphics_back_index);j.end();
    j.key("appearance");j.object();j.b("valid",snapshot.appearance_valid);U(appearance_generation);U(appearance_applied);U(appearance_status);
    const unsigned reason=c.appearance_error&65535;const int observed=character_appearance_observed(c.appearance_error);
    j.u("error_raw",c.appearance_error);j.u("reason",reason);j.s("reason_name",reason_name(reason));
    if(observed>=0)j.i("observed",observed);else j.null("observed");
    const auto stage=unsigned(character_loading_stage(c.appearance_error));
    j.u("loading_stage",stage);j.s("loading_stage_name",loading_name(stage));j.u("loading_observed",character_loading_observed(c.appearance_error));
    if(reason==unsigned(CharacterAppearanceError::ReadingAsset)&&observed>=0&&observed<=100)j.i("progress_percent",observed);else j.null("progress_percent");j.end();
    j.key("popup");j.object();j.u("raw_word",c.graphics_reserved);j.u("producer",c.graphics_reserved&255);j.u("hud",(c.graphics_reserved>>8)&255);
    j.u("events_byte",(c.graphics_reserved>>16)&255);j.u("frames_byte",c.graphics_reserved>>24);
    const bool cached_readable=snapshot.popup_cached_valid
        &&live_diagnostic_bounds_readable(snapshot.popup_cached_viewport.data(),snapshot.popup_cached_scissor.data());
    const bool ordered_readable=snapshot.popup_capture_stable&&snapshot.popup_capture_bounds_valid
        &&live_diagnostic_bounds_readable(c.popup_viewport,c.popup_scissor);
    j.key("cached");j.object();j.b("valid",cached_readable);
    j.b("usable",cached_readable&&live_diagnostic_bounds_usable(snapshot.popup_cached_viewport.data(),snapshot.popup_cached_scissor.data()));
    if(cached_readable){j.floats("viewport",snapshot.popup_cached_viewport.data(),4);j.integers("scissor",snapshot.popup_cached_scissor.data(),4);
        j.b("scissor_disabled",native_scissor_disabled(snapshot.popup_cached_scissor.data()));}
    else {j.null("viewport");j.null("scissor");}j.end();
    j.key("gpu_ordered");j.object();j.b("valid",ordered_readable&&c.popup_capture_status==1);
    j.b("stable",snapshot.popup_capture_stable);j.b("stale",snapshot.popup_capture_stable&&c.popup_capture_status==5);
    j.b("usable",ordered_readable&&c.popup_capture_status==1&&live_diagnostic_bounds_usable(c.popup_viewport,c.popup_scissor));
    U(popup_capture_sequence);U(popup_capture_status);U(popup_tick);U(popup_event);
    if(ordered_readable){j.floats("viewport",c.popup_viewport,4);j.integers("scissor",c.popup_scissor,4);
        j.b("scissor_disabled",native_scissor_disabled(c.popup_scissor));}
    else {j.null("viewport");j.null("scissor");}j.end();j.end();
    j.key("settings");j.object();j.b("valid",snapshot.settings_valid);U(settings_source);U(settings_attempts);U(settings_mount_result);U(settings_read_result);
    char raw[kSettingsBytes*2+1];constexpr char hex[]="0123456789abcdef";
    for(unsigned i=0;i<kSettingsBytes;++i){raw[2*i]=hex[c.settings[i]>>4];raw[2*i+1]=hex[c.settings[i]&15];}raw[kSettingsBytes*2]=0;
    j.s("raw_hex",raw,sizeof(raw));
    if(snapshot.settings_valid){j.key("decoded");j.object();settings_json(j,snapshot.settings);j.end();}else j.null("decoded");j.end();
    j.key("native");j.object();j.key("player");j.object();j.b("valid",snapshot.player_valid);
    if(snapshot.player_valid){j.integers("routine",snapshot.player_routine.data(),4);j.u("type",snapshot.player_type);j.u("bones",snapshot.player_bones);}
    j.end();j.key("camera");j.object();j.b("valid",snapshot.camera_valid);
    if(snapshot.camera_valid){j.floats("view",snapshot.camera_view.data(),12);j.floats("projection",snapshot.camera_projection.data(),16);}
    else {j.null("view");j.null("projection");}j.end();
    knife_json(j,snapshot.knife);hud_cached_json(j,snapshot.hud_cached);j.end();j.end();
#undef U
#undef F
#undef I
    return j.finish(size);
}
}
