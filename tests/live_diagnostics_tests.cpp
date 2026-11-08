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
    snapshot.process_found=snapshot.supported=snapshot.connected=true;snapshot.status=LiveDiagnosticStatus::Connected;
    snapshot.shared=initial_shared_control();snapshot.shared.status=1;
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
