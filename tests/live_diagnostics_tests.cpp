#include "re4/live_diagnostics.hpp"
#include <cassert>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <limits>
#include <regex>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#undef assert
#define assert(c) do {if(!(c)){std::fprintf(stderr,"line%d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
#endif

namespace {
// Independent JSON grammar check: rejects malformed literals, commas, numbers,
// control bytes, escapes and any trailing bytes. No dependency on the writer.
struct Parser {
    const std::string& text;std::size_t at=0;
    void ws(){while(at<text.size()&&(text[at]==' '||text[at]=='\n'||text[at]=='\r'||text[at]=='\t'))++at;}
    bool take(char c){ws();if(at<text.size()&&text[at]==c){++at;return true;}return false;}
    bool string(){
        if(!take('"'))return false;
        while(at<text.size()){
            const unsigned char c=static_cast<unsigned char>(text[at++]);
            if(c=='"')return true;
            if(c<32||c>=128)return false;
            if(c=='\\'){
                if(at==text.size())return false;
                const auto e=text[at++];
                if(e=='u'){for(unsigned i=0;i<4;++i){if(at==text.size()||!std::isxdigit(static_cast<unsigned char>(text[at++])))return false;}}
                else if(std::string("\"\\/bfnrt").find(e)==std::string::npos)return false;
            }
        }
        return false;
    }
    bool value(){
        ws();if(at==text.size())return false;
        if(text[at]=='"')return string();
        if(take('{')){if(take('}'))return true;do{if(!string()||!take(':')||!value())return false;}while(take(','));return take('}');}
        if(take('[')){if(take(']'))return true;do{if(!value())return false;}while(take(','));return take(']');}
        for(const char* literal:{"true","false","null"}){
            const auto n=std::strlen(literal);if(text.compare(at,n,literal)==0){at+=n;return true;}
        }
        const auto start=at;while(at<text.size()&&std::string("0123456789.eE+-").find(text[at])!=std::string::npos)++at;
        static const std::regex number("-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?");
        return at>start&&std::regex_match(text.substr(start,at-start),number);
    }
};
void valid_json(const std::string& text){Parser parser{text};assert(parser.value());parser.ws();assert(parser.at==text.size());}
}
int main(){
    using namespace re4;
    std::array<char,live_diagnostic_json_limit+16> out{};std::size_t size=99;
    LiveDiagnosticSnapshot snapshot;
    assert(format_live_diagnostics(snapshot,out.data(),live_diagnostic_json_limit,size));
    assert(size&&size<live_diagnostic_json_limit&&std::strlen(out.data())==size);valid_json(out.data());
    assert(std::string(out.data()).find("\"connected\":false")!=std::string::npos);
    assert(std::string(out.data()).find("\"health_reference\":null")!=std::string::npos);
    assert(std::string(out.data()).find("\"health_last_capture\":null")!=std::string::npos);
    assert(std::string(out.data()).find("\"module_observation\":null")!=std::string::npos);
    {
        LiveDiagnosticSnapshot failed;failed.status=LiveDiagnosticStatus::ModuleMissing;failed.process_found=failed.supported=true;
        auto& module=failed.module_observation;module.found=module.stable=true;module.abi=24;module.status=2;module.capabilities=1;
        module.scan_reads=128;module.scan_bytes=512*1024;std::memcpy(module.version.data(),"0.28.18",8);
        auto& profile=failed.profile_observation;profile.attempted=profile.stable=profile.complete=true;
        profile.pages=profile.read_calls=128;profile.bytes=512*1024;profile.words_checked=2066;profile.pointers_checked=69;
        profile.mismatch_count=99;profile.record_count=UINT32_MAX;
        for(unsigned i=0;i<8;++i){auto& mismatch=profile.mismatches[i];mismatch.rva=0x123400+i*4;mismatch.expected=UINT32_MAX;
            mismatch.observed=UINT32_MAX;mismatch.in_bounds=mismatch.readable=true;mismatch.pointer=i&1;mismatch.observed_in_main=false;}
        assert(format_live_diagnostics(failed,out.data(),out.size(),size));valid_json(out.data());
        const std::string raw(out.data());
        assert(raw.find("\"module_observation\":{\"schema\":1,\"found\":true,\"stable\":true,\"accepted_as_connection\":false")!=std::string::npos);
        assert(raw.find("\"version\":\"0.28.18\",\"abi\":24,\"status\":2")!=std::string::npos);
        assert(raw.find("\"connected\":false")!=std::string::npos&&raw.find("\"module\":null")!=std::string::npos);
        assert(raw.find("\"scope\":\"legacy_v18_extra_requirements\"")!=std::string::npos);
        assert(raw.find("\"observed_word\":4294967295")!=std::string::npos);
        assert(raw.find("\"observed_in_main\":false,\"observed_null\":false,\"observed_rva\":null")!=std::string::npos);
        assert(raw.find("address")==std::string::npos&&size<live_diagnostic_json_limit);
        failed.supported=false;
        assert(format_live_diagnostics(failed,out.data(),out.size(),size));
        assert(std::string(out.data()).find("\"module_observation\":null,\"profile_observation\":null")!=std::string::npos);
    }
    {
        LiveDiagnosticSnapshot limited;limited.status=LiveDiagnosticStatus::DiscoveryLimited;limited.process_found=limited.supported=true;
        limited.module_observation.scan_limit=true;limited.module_observation.scan_reads=115;limited.module_observation.scan_bytes=523784;
        assert(format_live_diagnostics(limited,out.data(),out.size(),size));valid_json(out.data());
        const std::string text(out.data());assert(text.find("\"status\":\"discovery_limited\"")!=std::string::npos);
        assert(text.find("\"found\":false")!=std::string::npos&&text.find("\"scan_limit\":true")!=std::string::npos);
        assert(text.find("\"version\":null")!=std::string::npos&&text.find("\"connected\":false")!=std::string::npos);
    }
    snapshot.process_found=snapshot.supported=snapshot.connected=true;snapshot.status=LiveDiagnosticStatus::Connected;
    snapshot.shared=initial_shared_control();snapshot.shared.status=1;
    auto& health=snapshot.health_reference;health.status=HealthReportStatus::Collected;
    health.settings_valid=health.settings_applied=health.bars_on=health.numbers_on=health.observed_consistent=true;
    health.read_mask=255;health.mode=255;health.rank=8;health.weapon_index=2;health.type=16;health.subtype=3;
    health.vptr_in_main=health.update_in_main=true;health.vptr_rva=0xAE7050;health.update_rva=0x8F470;health.table_rva=0xAF055C;
    health.current=1500;health.maximum=3000;health.coefficient=150;health.predicted_unit=62;
    health.settings_sequence=4;health.applied_sequence=4;

    snapshot.shared.frames=std::numeric_limits<std::uint64_t>::max();snapshot.shared.capabilities=0xFFFFFFFF;
    std::memcpy(snapshot.shared.module_version,"v\"\\\n\x80",6);
    snapshot.shared.raw_lx=std::numeric_limits<float>::quiet_NaN();
    snapshot.shared.graphics_native_far=std::numeric_limits<float>::infinity();
    snapshot.settings_valid=snapshot.appearance_valid=true;
    snapshot.shared.appearance_error=std::uint32_t(character_appearance_diagnostic(CharacterAppearanceError::ReadingAsset,67));
    snapshot.popup_capture_stable=snapshot.popup_capture_bounds_valid=true;
    snapshot.shared.popup_capture_status=5;snapshot.shared.popup_viewport[2]=1280;snapshot.shared.popup_viewport[3]=720;
    snapshot.shared.popup_scissor[2]=1280;snapshot.shared.popup_scissor[3]=720;
    snapshot.settings.turn_gain=std::numeric_limits<float>::quiet_NaN();
    snapshot.player_valid=snapshot.camera_valid=true;snapshot.camera_projection[3]=std::numeric_limits<float>::infinity();
    assert(format_live_diagnostics(snapshot,out.data(),live_diagnostic_json_limit,size));valid_json(out.data());
    const std::string text(out.data());
    assert(text.find("\"health_reference\":{\"schema\":1,\"predictor_version\":18")!=std::string::npos);
    assert(text.find("\"predicted_unit\":62")!=std::string::npos);
    assert(text.find("\"mode\":255,\"rank\":8,\"weapon_index\":2")!=std::string::npos);
    assert(text.find("\"settings_applied\":true")!=std::string::npos);
    assert(text.find("\"current\":1500,\"maximum\":3000")!=std::string::npos);

    assert(text.find("\"emitters\":null")!=std::string::npos);
    assert(text.find("\"positions_sample_readable\":false")!=std::string::npos);
    assert(text.find("\"knife\":{\"valid\":false")!=std::string::npos);
    assert(text.find("\"hud_cached\":{\"valid\":false,\"ordered_with_popup\":false")!=std::string::npos);
    assert(text.find("18446744073709551615")!=std::string::npos);
    assert(text.find("v\\\"\\\\\\u000a\\u0080")!=std::string::npos);
    assert(text.find("\"raw_lx\":null")!=std::string::npos);
    assert(text.find("\"graphics_native_far\":null")!=std::string::npos);
    assert(text.find("\"progress_percent\":67")!=std::string::npos);
    assert(text.find("\"stale\":true")!=std::string::npos);
    assert(text.find("\"valid\":false,\"stable\":true")!=std::string::npos);
    assert(text.find("0x")==std::string::npos);
    snapshot.shared.popup_capture_status=1;snapshot.popup_cached_valid=true;
    for(int observed:{0,-8,32768}){
        snapshot.shared.popup_viewport[2]=float(observed);snapshot.shared.popup_scissor[2]=observed;
        std::copy_n(snapshot.shared.popup_viewport,4,snapshot.popup_cached_viewport.data());
        std::copy_n(snapshot.shared.popup_scissor,4,snapshot.popup_cached_scissor.data());
        assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
        const std::string current(out.data());
        const auto usable=observed>0?"true":"false";
        assert(current.find(std::string("\"cached\":{\"valid\":true,\"usable\":")+usable)!=std::string::npos);
        assert(current.find(std::string("\"gpu_ordered\":{\"valid\":true,\"stable\":true,\"stale\":false,\"usable\":")+usable)!=std::string::npos);
        assert(current.find("\"viewport\":[0,0,"+std::to_string(observed)+",720]")!=std::string::npos);
    }
    const auto maximum=std::numeric_limits<std::int32_t>::max();
    snapshot.shared.popup_viewport[2]=1280;snapshot.shared.popup_viewport[3]=720;
    std::copy_n(snapshot.shared.popup_viewport,4,snapshot.popup_cached_viewport.data());
    for(unsigned i=0;i<4;++i)snapshot.shared.popup_scissor[i]=snapshot.popup_cached_scissor[i]=i<2?0:maximum;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string disabled(out.data());
    assert(disabled.find("\"cached\":{\"valid\":true,\"usable\":true")!=std::string::npos);
    assert(disabled.find("\"gpu_ordered\":{\"valid\":true,\"stable\":true,\"stale\":false,\"usable\":true")!=std::string::npos);
    assert(disabled.find("\"scissor\":[0,0,2147483647,2147483647]")!=std::string::npos);
    const auto ordered=disabled.find("\"gpu_ordered\":");
    assert(ordered!=std::string::npos&&disabled.find("\"scissor_disabled\":true",ordered)!=std::string::npos);
    assert(disabled.find("\"scissor\":[0,0,2147483647,2147483647]",ordered)!=std::string::npos);
    assert(disabled.find("\"scissor_disabled\":true")<ordered);
    // Fixed native observations are additive, pointer-free and remain bounded.
    snapshot.knife.valid=snapshot.knife.stable=true;
    snapshot.knife.player_valid=snapshot.knife.player_stable=snapshot.knife.vtable_in_main=true;
    snapshot.knife.player_vtable_offset=0xB0AAF8;snapshot.knife.player_bones=119;
    snapshot.knife.state_valid=true;snapshot.knife.weapon_id=0x23;
    snapshot.knife.body.valid=snapshot.knife.body.stable=snapshot.knife.body.chain_valid=true;
    snapshot.knife.body.face58.present=snapshot.knife.body.face58.valid=true;
    snapshot.knife.body.face58.flags=8;snapshot.knife.body.face58.alpha=255;
    snapshot.knife.body.face58.fields_readable=snapshot.knife.body.face58.vtable_in_main=true;
    snapshot.knife.body.face58.vtable_offset=0xB02088;snapshot.knife.body.face58.matrix_finite=true;
    snapshot.knife.body.face58.bin_vertices=snapshot.knife.body.face58.bin_normals=163;
    snapshot.knife.body.face58.bin_readable=true;snapshot.knife.body.face58.bin_header_3b=0xA0;
    snapshot.knife.body.source_shape_matches=true;
    auto& buffers=snapshot.knife.body.buffers;buffers.slot_readable=buffers.slot_valid=buffers.stable=true;buffers.slot=2;
    buffers.positions_present=buffers.positions_readable=buffers.positions_finite=true;
    buffers.normals_present=buffers.normals_readable=buffers.normals_finite=true;
    for(auto& h:snapshot.knife.held){
        h.present=h.valid=h.stable=h.vtable_in_main=h.root_matrix_valid=true;
        h.vtable_offset=0xB0AAF8;h.root_matrix.fill(-987654321.f);
        h.part.present=h.part.valid=h.part.bin_readable=h.part.tpl_readable=true;
        h.part.flags=0xFFFFFFFF;h.part.alpha=255;
    }
    auto& hud=snapshot.hud_cached;hud.context_valid=hud.context_stable=true;hud.ordered=true;
    for(auto* matrix:{&hud.model,&hud.view,&hud.projection,&hud.correction,&hud.combined}){
        matrix->valid=true;matrix->values.fill(-987654321.f);
    }
    hud.logical_viewport.valid=hud.adjusted_viewport.valid=true;
    hud.logical_viewport.values={0,0,512,448,0,1};hud.adjusted_viewport.values={0,0,1280,720,0,1};
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string native(out.data());assert(size<live_diagnostic_json_limit);
    assert(native.find("\"knife\":{\"valid\":true,\"stable\":true")!=std::string::npos);
    assert(native.find("\"player_vtable_offset\":"+std::to_string(0xB0AAF8))!=std::string::npos);
    assert(native.find("\"held_secondary\":{\"valid\":true")!=std::string::npos);
    assert(native.find("\"hud_cached\":{\"valid\":true,\"ordered_with_popup\":false")!=std::string::npos);
    assert(native.find("\"logical_viewport\":[0,0,512,448,0,1]")!=std::string::npos);
    assert(native.find("\"matrix_layout\":\"column_major\"")!=std::string::npos);
    assert(native.find("0x")==std::string::npos);
    assert(native.find("\"bin_vertices\":163")!=std::string::npos);
    assert(native.find("\"source_shape_metadata_only\":true")!=std::string::npos);
    assert(native.find("\"positions_sample_finite\":true")!=std::string::npos);
    assert(native.find("\"samples_only\":true")!=std::string::npos);
    snapshot.module_observation.found=snapshot.module_observation.stable=true;
    snapshot.module_observation.abi=24;snapshot.module_observation.status=1;snapshot.module_observation.capabilities=UINT32_MAX;
    snapshot.module_observation.frames=UINT64_MAX;std::memcpy(snapshot.module_observation.version.data(),"0.28.19",8);
    snapshot.profile_observation.attempted=snapshot.profile_observation.stable=snapshot.profile_observation.complete=true;
    snapshot.profile_observation.record_count=8;snapshot.profile_observation.mismatch_count=UINT32_MAX;
    snapshot.profile_observation.pages=snapshot.profile_observation.read_calls=128;snapshot.profile_observation.bytes=512*1024;
    for(unsigned i=0;i<8;++i){auto& m=snapshot.profile_observation.mismatches[i];m.rva=m.expected=m.observed=UINT32_MAX;
        m.pointer=i&1;m.readable=m.in_bounds=m.observed_in_main=true;}
    snapshot.emitters_valid=true;snapshot.emitters=initial_emitter_diagnostics();snapshot.emitters.sequence=8;
    for(auto& record:snapshot.emitters.records){
        for(auto& word:record)word=0xFFFFFFFF;
        const float visual[]{-12.5f,0.25f,19.f},fresh[]{1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
        std::memcpy(record+12,visual,sizeof(visual));std::memcpy(record+15,fresh,sizeof(fresh));
    }
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string emitter(out.data());assert(size<live_diagnostic_json_limit);
    assert(emitter.find("\"emitters\":{\"version\":1,\"sequence\":8")!=std::string::npos);
    for(const char* channel:{"laser","body_hand","held","emitter"})
        assert(emitter.find(std::string("\"")+channel+"\":{\"attempts\":4294967295")!=std::string::npos);
    assert(emitter.find("\"visual_delta\":[-12.5,0.25,19]")!=std::string::npos);
    assert(emitter.find("\"fresh_delta\":[1,null,null]")!=std::string::npos);
    // Actual retained producer record is distinct from the live best-effort candidate.
    auto& capture=snapshot.health_last_capture;capture=initial_health_capture();capture.sequence=10;capture.valid=1;
    snapshot.health_last_capture_valid=true;HealthCaptureSample sample;
    using CF=HealthCaptureField;
    sample.set(CF::ViewFrameLo,900);sample.set(CF::ViewFrameHi,1);
    sample.set(CF::ControlFrameLo,100);sample.set(CF::ControlFrameHi,0);
    sample.set(CF::TargetKeyLo,UINT32_MAX);sample.set(CF::TargetKeyHi,123);
    sample.set(CF::Health,std::uint32_t(std::uint16_t(-12))|(std::uint32_t(3000)<<16));
    sample.set(CF::WeaponBefore,3);sample.set(CF::WeaponAfter,4);sample.set(CF::ComputedUnit,58);sample.set(CF::PublishedUnit,0);
    sample.set(CF::FailedGate,unsigned(HealthReportGate::None));sample.set(CF::Outcome,2);
    sample.set(CF::Mode,255);sample.set(CF::Rank,10);sample.set(CF::WeaponIndex,2);
    sample.set(CF::Coefficient,std::uint32_t(-1));sample.set(CF::LinkMask,3);sample.set(CF::VptrRva,0xAE7050);
    std::memcpy(capture.words,sample.words,sizeof(sample.words));snapshot.shared.frames=104;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string retained(out.data());assert(size<live_diagnostic_json_limit);
    assert(retained.find("\"health_last_capture\":{\"schema\":1,\"sequence\":10")!=std::string::npos);
    assert(retained.find("\"retained\":true,\"provider_observation\":true")!=std::string::npos);
    assert(retained.find("\"capture_view_frame\":4294968196,\"capture_control_frame\":100,\"current_module_frame\":104")!=std::string::npos);
    assert(retained.find("\"age_comparable\":true,\"age_frames\":4")!=std::string::npos);
    assert(retained.find("\"target_key_lo\":4294967295,\"target_key_hi\":123")!=std::string::npos);
    assert(retained.find("\"current\":-12,\"maximum\":3000")!=std::string::npos);
    assert(retained.find("\"weapon_stamp_before\":3,\"weapon_stamp_after\":4,\"computed_unit\":58,\"published_unit\":0")!=std::string::npos);
    assert(retained.find("\"first_failed_gate\":0,\"gate_name\":\"none\",\"outcome\":2,\"outcome_name\":\"stamp_mismatch\"")!=std::string::npos);
    assert(retained.find("\"predicted_unit\":62")!=std::string::npos);
    assert(retained.find("\"vptr_rva\":"+std::to_string(0xAE7050)+",\"update_rva\":null,\"table_rva\":null,\"coefficient\":-1")!=std::string::npos);
    assert(retained.find("\"native_guard\":null")!=std::string::npos); // Unmodified17/18 record has no guard bits.
    sample.set(CF::GuardRva,0x90508);sample.set(CF::GuardExpected,0x12345678);sample.set(CF::GuardObserved,UINT32_MAX);
    std::memcpy(capture.words,sample.words,sizeof(sample.words));
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    assert(std::string(out.data()).find("\"native_guard\":{\"rva\":"+std::to_string(0x90508)+",\"expected\":305419896,\"observed\":4294967295}")!=std::string::npos);
    capture.words[unsigned(CF::ReadMask)]&=~(std::uint32_t(1)<<unsigned(CF::GuardObserved));
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));
    assert(std::string(out.data()).find("\"native_guard\":null")!=std::string::npos);
    sample.set(CF::GuardRva,UINT32_MAX);std::memcpy(capture.words,sample.words,sizeof(sample.words));
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));
    assert(std::string(out.data()).find("\"native_guard\":null")!=std::string::npos);
    snapshot.shared.frames=99;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    assert(std::string(out.data()).find("\"age_comparable\":false,\"age_frames\":null")!=std::string::npos);
    capture.words[unsigned(CF::ReadMask)]&=~(std::uint32_t(1)<<unsigned(CF::ControlFrameHi));snapshot.shared.frames=104;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));
    assert(std::string(out.data()).find("\"age_comparable\":false,\"age_frames\":null")!=std::string::npos);
    // The fixed live candidate and actual retained provider tuple remain distinct observations.
    const auto before_actual_tuple=health;
    health.type=0x1E;health.subtype=0x17;health.body_status=2;health.phase=15;health.mode=6;health.rank=10;
    health.weapon_index=2;health.coefficient=150;health.current=health.maximum=1500;health.predicted_unit=58;
    sample.set(CF::Type,0x1E);sample.set(CF::Subtype,0x17);sample.set(CF::BodyStatus,2);sample.set(CF::Phase,15);
    sample.set(CF::Mode,6);sample.set(CF::Rank,10);sample.set(CF::WeaponIndex,2);sample.set(CF::Coefficient,150);
    sample.set(CF::ComputedUnit,58);sample.set(CF::PublishedUnit,58);sample.set(CF::Outcome,1);
    sample.set(CF::WeaponAfter,3);sample.set(CF::LinkMask,0);sample.set(CF::Health,0x05DC05DC);
    std::memcpy(capture.words,sample.words,sizeof(sample.words));
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string actual_tuple(out.data());
    assert(actual_tuple.find("\"health_reference\":{\"schema\":1,\"predictor_version\":18")!=std::string::npos);
    assert(actual_tuple.find("\"type\":30,\"subtype\":23,\"current\":1500,\"maximum\":1500")!=std::string::npos);
    assert(actual_tuple.find("\"predicted_unit\":58")!=std::string::npos);
    assert(actual_tuple.find("\"computed_unit\":58,\"published_unit\":58")!=std::string::npos);
    assert(actual_tuple.find("\"first_failed_gate\":0,\"gate_name\":\"none\",\"outcome\":1,\"outcome_name\":\"published\"")!=std::string::npos);
    health.subtype=6;health.predicted_unit=0;health.first_failed_gate=HealthReportGate::Prediction;
    health.rejected_mask=health_report_gate_bit(HealthReportGate::Prediction);
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string immune_tuple(out.data());
    assert(immune_tuple.find("\"status_name\":\"collected\",\"first_failed_gate\":15,\"gate_name\":\"prediction\"")!=std::string::npos);
    assert(immune_tuple.find("\"type\":30,\"subtype\":6,\"current\":1500,\"maximum\":1500")!=std::string::npos);
    assert(immune_tuple.find("\"predicted_unit\":0")!=std::string::npos);
    assert(immune_tuple.find("\"computed_unit\":58,\"published_unit\":58")!=std::string::npos);
    health.type=0x22;health.subtype=6;health.body_status=255;health.phase=4;
    health.vptr_rva=0xAE8170;health.update_rva=0xDF550;health.predicted_unit=127;
    health.first_failed_gate=HealthReportGate::None;health.rejected_mask=0;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string plain_family(out.data());
    assert(plain_family.find("\"type\":34,\"subtype\":6,\"current\":1500,\"maximum\":1500")!=std::string::npos);
    assert(plain_family.find("\"body_status\":255")!=std::string::npos);
    assert(plain_family.find("\"vptr_rva\":"+std::to_string(0xAE8170)+",\"update_rva\":"+std::to_string(0xDF550))!=std::string::npos);
    assert(plain_family.find("\"predicted_unit\":127")!=std::string::npos);
    assert(plain_family.find("\"computed_unit\":58,\"published_unit\":58")!=std::string::npos);
    health=before_actual_tuple;
    for(unsigned invalid=0;invalid<6;++invalid){
        auto bad=initial_health_capture();bad.sequence=2;bad.valid=1;
        if(invalid==0)bad.marker[0]^=1;
        if(invalid==1)bad.version=2;
        if(invalid==2)bad.bytes=159;
        if(invalid==3)bad.sequence=3;
        if(invalid==4)bad.valid=0;
        if(invalid==5)bad.valid=2;
        capture=bad;assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
        assert(std::string(out.data()).find("\"health_last_capture\":null")!=std::string::npos);
    }
    capture=initial_health_capture();capture.sequence=2;capture.valid=1;
    for(auto& word:capture.words)word=UINT32_MAX;
    capture.words[unsigned(CF::GuardRva)]=UINT32_MAX-1; // Worst valid guard triplet remains inside the same16KiBcanaries.
    assert(format_live_diagnostics(snapshot,out.data(),live_diagnostic_json_limit,size));valid_json(out.data());
    assert(size<live_diagnostic_json_limit);
    assert(std::string(out.data()).find("\"outcome_name\":\"unknown\"")!=std::string::npos);
    snapshot.emitters.sequence=9;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    assert(std::string(out.data()).find("\"emitters\":null")!=std::string::npos);
    snapshot.emitters.sequence=8;snapshot.emitters.bytes=999;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));
    assert(std::string(out.data()).find("\"emitters\":null")!=std::string::npos);
    // Missing target still exports the observed global context; unknown fields are explicit null RVAs.
    health.status=HealthReportStatus::NoTarget;health.first_failed_gate=HealthReportGate::Target;
    health.read_mask=HealthSettings|HealthContext|HealthPool|HealthGlobals;health.target_slot=UINT32_MAX;
    health.vptr_in_main=health.update_in_main=false;health.vptr_rva=health.update_rva=health.table_rva=UINT32_MAX;
    health.predicted_unit=0;health.coefficient=-1;health.current=-32768;health.maximum=32767;
    health.observed_consistent=false;health.flags=UINT32_MAX;health.rejected_mask=UINT32_MAX;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string absent_target(out.data());
    const auto hp=absent_target.find("\"health_reference\":");assert(hp!=std::string::npos);
    const auto health_end=absent_target.find('}',hp);const auto health_text=absent_target.substr(hp,health_end-hp+1);
    assert(health_text.find("\"status_name\":\"no_target\"")!=std::string::npos);
    assert(health_text.find("\"gate_name\":\"target\"")!=std::string::npos);
    assert(health_text.find("\"mode\":255,\"rank\":8,\"weapon_index\":2")!=std::string::npos);
    assert(health_text.find("\"vptr_rva\":null,\"update_rva\":null,\"table_rva\":null")!=std::string::npos);
    assert(health_text.find("\"coefficient\":-1,\"predicted_unit\":0")!=std::string::npos);
    assert(health_text.find("\"current\":-32768,\"maximum\":32767")!=std::string::npos);
    assert(health_text.find("\"captured_hud_unit\":false,\"best_effort\":true")!=std::string::npos);
    assert(health_text.find("pointer")==std::string::npos&&health_text.find("address")==std::string::npos);
    health.status=static_cast<HealthReportStatus>(UINT32_MAX);health.first_failed_gate=static_cast<HealthReportGate>(UINT32_MAX);
    health.schema=health.predictor_version=health.ipc_reads=health.settings_sequence=health.applied_sequence=UINT32_MAX;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    assert(size<live_diagnostic_json_limit&&std::string(out.data()).find("\"status_name\":\"unknown\"")!=std::string::npos);
    health={};
    snapshot.emitters=initial_emitter_diagnostics();
    hud.context_stable=false;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    assert(std::string(out.data()).find("\"logical_viewport\":null")!=std::string::npos);
    assert(std::string(out.data()).find("\"correction\":null")!=std::string::npos);
    snapshot.popup_capture_stable=false;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    const std::string unstable(out.data());const auto unstable_ordered=unstable.find("\"gpu_ordered\":");
    assert(unstable.find("\"gpu_ordered\":{\"valid\":false,\"stable\":false,\"stale\":false,\"usable\":false")!=std::string::npos);
    assert(unstable.find("\"viewport\":null,\"scissor\":null",unstable_ordered)!=std::string::npos);
    assert(unstable.find("\"scissor_disabled\":true",unstable_ordered)==std::string::npos);
    snapshot.popup_capture_stable=true;snapshot.shared.popup_capture_status=5;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    assert(std::string(out.data()).find("\"gpu_ordered\":{\"valid\":false,\"stable\":true,\"stale\":true,\"usable\":false")!=std::string::npos);
    assert(std::string(out.data()).find("\"scissor\":[0,0,2147483647,2147483647]")!=std::string::npos);
    snapshot.shared.popup_capture_status=1;snapshot.shared.popup_viewport[2]=0;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));
    assert(std::string(out.data()).find("\"gpu_ordered\":{\"valid\":true,\"stable\":true,\"stale\":false,\"usable\":false")!=std::string::npos);
    snapshot.shared.popup_viewport[2]=1280;
    for(const std::array<std::int32_t,4> bad:{std::array<std::int32_t,4>{1,0,maximum,maximum},
            {0,-1,maximum,maximum},{0,0,maximum,720},{0,0,1280,maximum},{0,0,maximum-1,maximum},{0,0,65537,720}}){
        std::copy(bad.begin(),bad.end(),snapshot.shared.popup_scissor);snapshot.popup_cached_scissor=bad;
        assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
        const std::string invalid(out.data());
        assert(invalid.find("\"cached\":{\"valid\":false,\"usable\":false,\"viewport\":null")!=std::string::npos);
        assert(invalid.find("\"gpu_ordered\":{\"valid\":false")!=std::string::npos);
        assert(invalid.find("\"scissor_disabled\":true")==std::string::npos);
    }
    for(unsigned i=0;i<4;++i)snapshot.shared.popup_scissor[i]=snapshot.popup_cached_scissor[i]=i<2?0:maximum;
    snapshot.shared.popup_viewport[0]=snapshot.popup_cached_viewport[0]=std::numeric_limits<float>::quiet_NaN();
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    assert(std::string(out.data()).find("\"cached\":{\"valid\":false,\"usable\":false,\"viewport\":null")!=std::string::npos);
    assert(std::string(out.data()).find("\"gpu_ordered\":{\"valid\":false")!=std::string::npos);
    out.fill('Q');size=55;
    assert(!format_live_diagnostics(snapshot,out.data(),16,size)&&size==0&&out[0]==0&&out[16]=='Q');
    assert(!format_live_diagnostics(snapshot,nullptr,100,size)&&size==0);
    assert(!format_live_diagnostics(snapshot,out.data(),0,size)&&size==0);
    snapshot.status=static_cast<LiveDiagnosticStatus>(255);
    snapshot.shared.popup_capture_status=0xFFFFFFFF;
    assert(format_live_diagnostics(snapshot,out.data(),out.size(),size));valid_json(out.data());
    assert(size<live_diagnostic_json_limit);
    std::puts("live diagnostics: bounded JSON, fixed observations, finite numbers, escapes and absent game PASS");
}
