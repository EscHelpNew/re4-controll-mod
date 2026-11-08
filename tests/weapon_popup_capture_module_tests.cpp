#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#include "native_popup_memory.hpp"

namespace {
alignas(8) std::uint8_t queued_capture[0x30]{};
unsigned capture_allocations=0,end_calls=0,end_at_enqueue=0,software_index=0;
bool null_context=false;
unsigned expected_vertices=0,emitted_vertices=0;
void capture_begin(unsigned primitive,unsigned,unsigned count){assert((primitive==0x98&&count==4)||(primitive==0x90&&count%6==0));expected_vertices=count;emitted_vertices=0;}
void capture_position(float,float,float){++emitted_vertices;}
void capture_color(unsigned,unsigned,unsigned,unsigned){}
void capture_end(unsigned token){assert(token==0x34&&emitted_vertices==expected_vertices);++end_calls;}
void* capture_allocate(std::uint64_t size){
    assert(size==sizeof(queued_capture)&&end_calls>0);++capture_allocations;end_at_enqueue=end_calls;
    std::memset(queued_capture,0,sizeof(queued_capture));character_store(queued_capture,8,std::uint16_t(size));character_store(queued_capture,0xA,std::uint16_t(7));return queued_capture;
}
void* capture_context(unsigned id){assert(id==7);return null_context?nullptr:&software_index;}
struct CaptureFixture:Fixture {
    NativePopupMemory popup_memory;
    std::array<std::uint8_t,0xFC0> graphics{};
    CaptureFixture(){
        view_mode();settings.dpad_weapon_switching=true;publish_view_settings();
        native_gx_begin=capture_begin;native_gx_position=capture_position;native_gx_color=capture_color;native_gx_end=capture_end;
        weapon_popup_capture_native={capture_allocate,capture_context};
        weapon_popup_capture_frame.store(~std::uint64_t(0));
        capture_allocations=end_calls=end_at_enqueue=software_index=0;null_context=false;game_test::tick=100;view_frame.store(0);
        character_store(image.data(),0x1180D40+0x140,static_cast<void*>(graphics.data()));
        game_test::memory_maps={{main_base,image.size(),1},{reinterpret_cast<std::uintptr_t>(graphics.data()),graphics.size(),1},
            {reinterpret_cast<std::uintptr_t>(&software_index),sizeof(software_index),1}};
        popup_memory.bind(*this);
        viewport({0,0,1280,720});scissor({0,0,1280,720});
        control.popup_capture_sequence=0;control.popup_capture_status=0;
        for(unsigned i=0;i<4;++i){control.popup_viewport[i]=-11;control.popup_scissor[i]=-22;}
        control.popup_tick=13;control.popup_event=17;
    }
    void viewport(std::array<float,4> value){std::memcpy(graphics.data()+0xF80,value.data(),16);}
    void scissor(std::array<std::int32_t,4> value){std::memcpy(graphics.data()+0xFA0,value.data(),16);}
    void draw(){HudDraw::Callback(nullptr);}
    void event(){weapon_popup_commit(actor,35);game_test::tick+=9'600'000;}
    void worker(){game_test::tick+=19;character_value<void(*)(void*)>(queued_capture,0)(queued_capture);}
};
}
int main(){
    CaptureFixture f;
    f.draw();assert(!capture_allocations); // No event.
    weapon_popup_commit(f.actor,35);f.draw();assert(!capture_allocations); // Alpha zero.
    game_test::tick+=9'600'000;f.draw();assert(capture_allocations==1&&end_at_enqueue==end_calls);
    assert(control.popup_capture_sequence==0&&control.popup_capture_status==0&&control.popup_viewport[0]==-11&&control.popup_scissor[0]==-22&&control.popup_tick==13&&control.popup_event==17);
    const auto event_sequence=character_value<std::uint64_t>(queued_capture,0x18);
    assert(event_sequence&&!(event_sequence&1)&&character_value<std::uint16_t>(queued_capture,8)==0x30&&character_value<std::uint16_t>(queued_capture,0xA)==7);
    f.viewport({3,4,1920,1080});f.scissor({7,8,1910,1000});
    const auto gpu_before=f.graphics;f.worker();assert(control.popup_capture_sequence==2&&control.popup_capture_status==1);
    assert(control.popup_viewport[0]==3&&control.popup_viewport[2]==1920&&control.popup_scissor[1]==8&&control.popup_scissor[3]==1000);
    assert(control.popup_tick==game_test::tick&&control.popup_event==event_sequence&&f.graphics==gpu_before);
    f.draw();assert(capture_allocations==1); // Once per initial frame0.
    view_frame.fetch_add(1);f.draw();assert(capture_allocations==2);f.worker();assert(control.popup_capture_sequence==4);
    // A newly committed event cannot relabel an older command's capture.
    view_frame.fetch_add(1);f.draw();const auto old_event=character_value<std::uint64_t>(queued_capture,0x18);
    weapon_popup_commit(f.actor,44);f.worker();assert(control.popup_capture_status==5&&control.popup_event==old_event);
    f.event();view_frame.fetch_add(1);f.draw();weapon_popup_clear();f.worker();assert(control.popup_capture_status==5);
    f.event();view_frame.fetch_add(1);f.draw();software_index=1;f.worker();assert(control.popup_capture_status==2&&control.popup_viewport[2]==0&&control.popup_scissor[2]==0);
    software_index=0;view_frame.fetch_add(1);f.draw();null_context=true;f.worker();assert(control.popup_capture_status==2);null_context=false;
    view_frame.fetch_add(1);f.draw();f.viewport({0,0,std::numeric_limits<float>::quiet_NaN(),720});f.worker();assert(control.popup_capture_status==3&&control.popup_viewport[2]==0&&control.popup_scissor[2]==1910);
    view_frame.fetch_add(1);f.draw();f.scissor({0,0,1000000,720});f.worker();assert(control.popup_capture_status==6);
    f.viewport({0,0,1280,720});view_frame.fetch_add(1);f.draw();f.worker();assert(control.popup_capture_status==4&&control.popup_viewport[2]==1280&&control.popup_scissor[2]==0);
    f.scissor({0,0,1280,0});view_frame.fetch_add(1);f.draw();f.worker();assert(control.popup_capture_status==1&&control.popup_scissor[3]==0); // Empty clip is observable.
    // Native64E084..B0 emits this exact rectangle when scissor is disabled.
    const auto maximum=std::numeric_limits<std::int32_t>::max();
    f.scissor({0,0,maximum,maximum});view_frame.fetch_add(1);f.draw();
    const auto unbounded_before=f.graphics;f.worker();
    assert(control.popup_capture_status==1&&control.popup_scissor[0]==0&&control.popup_scissor[1]==0
        &&control.popup_scissor[2]==maximum&&control.popup_scissor[3]==maximum&&f.graphics==unbounded_before);
    f.viewport({0,0,std::numeric_limits<float>::quiet_NaN(),720});view_frame.fetch_add(1);f.draw();f.worker();
    assert(control.popup_capture_status==3&&control.popup_viewport[2]==0&&control.popup_scissor[2]==maximum);
    f.viewport({0,0,1280,720});
    view_frame.fetch_add(1);f.draw();weapon_popup_commit(f.actor,44);f.worker();
    assert(control.popup_capture_status==5&&control.popup_scissor[2]==maximum); // Stale remains attributable, never usable as current.
    f.event();
    for(const std::array<std::int32_t,4> bad:{std::array<std::int32_t,4>{1,0,maximum,maximum},
            {0,-1,maximum,maximum},{0,0,maximum,720},{0,0,1280,maximum},{0,0,maximum-1,maximum},
            {0,0,65537,720},{0,0,1280,-65537}}){
        f.scissor(bad);view_frame.fetch_add(1);f.draw();f.worker();
        assert(control.popup_capture_status==4&&control.popup_scissor[2]==0&&control.popup_scissor[3]==0);
    }
    f.scissor({0,0,maximum,maximum});
    view_frame.fetch_add(1);f.draw();
    character_store(f.image.data(),0x1180D40+0x140,reinterpret_cast<void*>(~std::uintptr_t(0)-1));
    f.worker();assert(control.popup_capture_status==2);
    character_store(f.image.data(),0x1180D40+0x140,static_cast<void*>(f.graphics.data()));
    view_frame.fetch_add(1);f.draw();game_test::tick=character_value<std::uint64_t>(queued_capture,0x10)-1;
    weapon_popup_capture_command(queued_capture);assert(control.popup_capture_status==5); // Tick rollback cannot attribute a future event.
    game_test::tick+=100;
    const auto stable_sequence=control.popup_capture_sequence;control.popup_capture_sequence=stable_sequence+1;
    view_frame.fetch_add(1);f.draw();f.worker();assert(control.popup_capture_sequence==stable_sequence+1); // No second writer crosses an odd snapshot.
    control.popup_capture_sequence=stable_sequence;
    weapon_popup_capture_native={};
    std::puts("weapon popup ordered capture callbacks passed");
}
