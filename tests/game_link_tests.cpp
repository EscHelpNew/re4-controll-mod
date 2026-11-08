#include "../source/overlay/game_link.cpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
namespace {
constexpr u64 address=0x100000;
std::array<u8,0x1000> memory{};
u64 pid=42,title=re4::profile::title_id,mapping_count=1;
unsigned attach_rc=0,attach_calls=0,write_count=0,fail_write=0,reads=0;
bool build_supported=true;
bool appearance_race=false;
bool capture_race=false,settings_race=false;
constexpr u64 gfx_pointer_offset=0x1180D40+0x140,gfx_address=0x600000;
u64 main_base=0,main_size=0,gfx_pointer=0,fail_read_address=0;
std::array<u8,0x30> cached_bounds{};
unsigned native_reads=0;
std::vector<std::size_t> write_sizes;
struct Segment {u64 address;std::vector<u8> bytes;};
std::vector<Segment> segments;
template<class T>void segment(u64 at,const T& value){
    Segment s{at,std::vector<u8>(sizeof(T))};std::memcpy(s.bytes.data(),&value,sizeof(T));segments.push_back(s);
}
re4::SharedControl snapshot(){
    re4::SharedControl block{};std::memcpy(&block,memory.data()+8,sizeof(block));return block;
}
void reset(){
    memory.fill(0);pid=42;title=re4::profile::title_id;mapping_count=1;
    attach_rc=attach_calls=write_count=fail_write=reads=0;build_supported=true;appearance_race=false;write_sizes.clear();
    main_base=main_size=gfx_pointer=fail_read_address=0;cached_bounds.fill(0);native_reads=0;
    capture_race=settings_race=false;segments.clear();
    re4::SharedControl block{};std::memcpy(block.marker,re4::kSharedMarker,16);
    block.abi=re4::kSharedAbi;block.status=1;block.capabilities=re4::kCapabilityTurning;block.sequence=4;
    block.fps_overrides=7;block.reticle_draws=9;block.modern_move_steps=11;block.modern_pose_steps=13;
    block.modern_move_status=3;block.modern_move_distance=12.5f;
    std::memcpy(memory.data()+8,&block,sizeof(block));
}
}
Result pminfoInitialize(){return 0;}void pminfoExit(){}
Result smGetService(Service* s,const char*){s->active=true;return 0;}
bool serviceIsActive(Service* s){return s->active;}void serviceClose(Service* s){s->active=false;}
Result pmdmntGetApplicationProcessId(u64* p){*p=pid;return 0;}
Result pminfoGetProgramId(u64* p,u64){*p=title;return 0;}
Result test_dispatch(Service*,unsigned command,const void* in,std::size_t,void* out,std::size_t,TestDispatchParams p){
    if(command==65003){++attach_calls;return attach_rc;}
    if(command==65002){
        auto* m=static_cast<Metadata*>(out);*m={};m->pid=pid;m->title=title;
        m->main={main_base,main_size};
        std::memcpy(m->build_id,re4::profile::build_id,32);if(!build_supported)m->build_id[0]^=1;
        return 0;
    }
    if(command==65101){
        const u64 offset=*static_cast<const u64*>(in);
        *static_cast<u64*>(out)=offset?0:mapping_count;
        auto* mappings=static_cast<MemoryInfo*>(const_cast<void*>(p.buffers[0].data));
        mappings[0]={address,memory.size(),MemType_CodeMutable,Perm_Rw};return 0;
    }
    assert(command==65102||command==65103);
    struct Range{u64 addr,size;};const auto r=*static_cast<const Range*>(in);
    if(command==65102){
        if(r.addr==fail_read_address)return 9;
        for(const auto& s:segments)if(r.addr>=s.address&&r.addr-s.address<=s.bytes.size()&&r.size<=s.bytes.size()-(r.addr-s.address)){
            std::memcpy(const_cast<void*>(p.buffers[0].data),s.bytes.data()+r.addr-s.address,r.size);return 0;
        }
    }
    if(command==65102&&main_base&&r.addr==main_base+gfx_pointer_offset){
        ++native_reads;assert(r.size==sizeof(gfx_pointer));
        if(r.addr==fail_read_address)return 9;
        std::memcpy(const_cast<void*>(p.buffers[0].data),&gfx_pointer,sizeof(gfx_pointer));return 0;
    }
    if(command==65102&&r.addr==gfx_address+0xF80){
        ++native_reads;assert(r.size==cached_bounds.size());
        if(r.addr==fail_read_address)return 9;
        std::memcpy(const_cast<void*>(p.buffers[0].data),cached_bounds.data(),cached_bounds.size());return 0;
    }
    if(r.addr<address||r.addr-address>memory.size()||r.size>memory.size()-(r.addr-address))return 7;
    auto* target=memory.data()+r.addr-address;
    if(command==65102){
        ++reads;std::memcpy(const_cast<void*>(p.buffers[0].data),target,r.size);
        if(appearance_race&&r.addr==address+8+offsetof(re4::SharedControl,appearance_generation)){
            auto block=snapshot();block.appearance_generation+=2;std::memcpy(memory.data()+8,&block,sizeof(block));
        }
        if(capture_race&&r.addr==address+8+offsetof(re4::SharedControl,popup_capture_sequence)){
            auto block=snapshot();block.popup_capture_sequence+=2;std::memcpy(memory.data()+8,&block,sizeof(block));
        }
        if(settings_race&&r.addr==address+8+offsetof(re4::SharedControl,sequence)){
            auto block=snapshot();block.sequence+=2;std::memcpy(memory.data()+8,&block,sizeof(block));
        }
    }
    else{
        ++write_count;write_sizes.push_back(r.size);
        if(fail_write==write_count)return 5;
        std::memcpy(target,p.buffers[0].data,r.size);
    }
    return 0;
}
int main(){
    {
        reset();GameLink worker;re4::LiveDiagnosticSnapshot live;
        assert(!worker.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::ServiceUnavailable);
        worker.initialize();
        auto block=snapshot();re4::Settings settings;settings.character_appearance=re4::CharacterAppearance::Ada;
        assert(re4::encode_settings(settings,block.settings,sizeof(block.settings)));
        block.appearance_generation=4;block.appearance_applied=2;block.appearance_status=2;
        block.popup_capture_sequence=8;block.popup_capture_status=1;block.popup_tick=123;block.popup_event=456;
        block.popup_viewport[2]=1280;block.popup_viewport[3]=720;block.popup_scissor[2]=1280;block.popup_scissor[3]=720;
        std::memcpy(block.module_version,"0.26.0",7);std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(worker.read_diagnostic_snapshot(live)&&live.connected&&live.supported&&live.process_found);
        assert(live.settings_valid&&live.settings.character_appearance==re4::CharacterAppearance::Ada&&live.appearance_valid);
        assert(live.popup_capture_stable&&live.popup_capture_bounds_valid&&live.shared.popup_tick==123);
        assert(!live.player_valid&&!live.camera_valid&&write_count==0);
        assert(attach_calls==1);
        block=snapshot();block.popup_capture_sequence|=1;std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(worker.read_diagnostic_snapshot(live)&&!live.popup_capture_stable&&attach_calls==1);
        block.popup_capture_sequence=10;block.popup_viewport[0]=std::numeric_limits<float>::quiet_NaN();
        std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(worker.read_diagnostic_snapshot(live)&&live.popup_capture_stable&&!live.popup_capture_bounds_valid);
        block.popup_viewport[0]=0;block.popup_capture_status=5;std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(worker.read_diagnostic_snapshot(live)&&live.popup_capture_stable&&live.popup_capture_bounds_valid);
        for(int observed:{0,-8,32768}){
            block.popup_capture_status=1;block.popup_viewport[2]=float(observed);block.popup_scissor[2]=observed;
            std::memcpy(memory.data()+8,&block,sizeof(block));
            assert(worker.read_diagnostic_snapshot(live)&&live.popup_capture_bounds_valid);
            assert(live.shared.popup_viewport[2]==float(observed)&&live.shared.popup_scissor[2]==observed);
        }
        const auto maximum=std::numeric_limits<std::int32_t>::max();
        block.popup_capture_status=1;block.popup_viewport[2]=1280;
        block.popup_scissor[0]=block.popup_scissor[1]=0;block.popup_scissor[2]=block.popup_scissor[3]=maximum;
        std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(worker.read_diagnostic_snapshot(live)&&live.popup_capture_stable&&live.popup_capture_bounds_valid&&live.shared.popup_scissor[2]==maximum);
        block.popup_capture_status=5;std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(worker.read_diagnostic_snapshot(live)&&live.popup_capture_stable&&live.popup_capture_bounds_valid&&live.shared.popup_capture_status==5);
        for(const std::array<std::int32_t,4> bad:{std::array<std::int32_t,4>{1,0,maximum,maximum},
                {0,-1,maximum,maximum},{0,0,maximum,720},{0,0,1280,maximum},{0,0,maximum-1,maximum},{0,0,65537,720}}){
            std::copy(bad.begin(),bad.end(),block.popup_scissor);std::memcpy(memory.data()+8,&block,sizeof(block));
            assert(worker.read_diagnostic_snapshot(live)&&live.popup_capture_stable&&!live.popup_capture_bounds_valid);
        }
        block.popup_scissor[0]=block.popup_scissor[1]=0;block.popup_scissor[2]=block.popup_scissor[3]=maximum;
        std::memcpy(memory.data()+8,&block,sizeof(block));
        capture_race=true;assert(worker.read_diagnostic_snapshot(live)&&!live.popup_capture_stable&&!live.popup_capture_bounds_valid);capture_race=false;
        appearance_race=true;assert(worker.read_diagnostic_snapshot(live)&&!live.appearance_valid&&live.settings_valid);appearance_race=false;
        settings_race=true;assert(worker.read_diagnostic_snapshot(live)&&!live.settings_valid);settings_race=false;
        main_base=0x7100000000;main_size=0x1200000;const u64 player=0x800000,state=0x900000;
        segment(main_base+re4::profile::player_slot,player);segment(main_base+0xB1AE08,state);
        const std::array<u8,8> fields{0,6,1,0,0,0,119,0};segment(player+0x114,fields);
        std::array<float,29> matrices{};matrices[0]=matrices[5]=matrices[10]=1;
        matrices[13]=matrices[18]=matrices[23]=matrices[28]=1;segment(state+0xD8,matrices);
        assert(worker.read_diagnostic_snapshot(live)&&live.player_valid&&live.player_routine[1]==6&&live.player_bones==119&&live.camera_valid);
        fail_read_address=state+0xD8;assert(worker.read_diagnostic_snapshot(live)&&live.player_valid&&!live.camera_valid);fail_read_address=0;
        ++pid;assert(worker.read_diagnostic_snapshot(live)&&live.connected&&write_count==0&&attach_calls==2); // reconnect to new process
        fail_read_address=address+8;
        assert(!worker.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::ReadFailed);
        fail_read_address=0;assert(worker.read_diagnostic_snapshot(live)&&attach_calls==3);
        build_supported=false;const auto old_reads=reads;
        assert(!worker.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::UnsupportedBuild&&!live.connected&&reads==old_reads);
        build_supported=true;pid=0;assert(!worker.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::NoGame);
        worker.close();
    }
    {
        reset();memory[8]=0;GameLink worker;worker.initialize();re4::LiveDiagnosticSnapshot live;
        assert(!worker.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::ModuleMissing&&live.supported&&!live.connected&&attach_calls==1);
        worker.close();
    }
    {
        reset();main_base=0x7100000000;main_size=0x1200000;gfx_pointer=gfx_address;
        const std::array<float,4> viewport{0,1.5f,1280,720};
        const std::array<std::int32_t,4> scissor{0,2,1280,718};
        std::memcpy(cached_bounds.data(),viewport.data(),sizeof(viewport));
        std::memcpy(cached_bounds.data()+0x20,scissor.data(),sizeof(scissor));
        GameLink link;link.initialize();assert(link.connect());
        auto block=snapshot();block.graphics_reserved=0xFF150503;std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(link.refresh_popup()&&link.popup_diagnostic==0xFF150503);
        assert(link.popup_bounds_valid&&link.popup_viewport==viewport&&link.popup_scissor==scissor&&native_reads==2);
        const auto maximum=std::numeric_limits<std::int32_t>::max();
        const std::array<std::int32_t,4> disabled{0,0,maximum,maximum};
        std::memcpy(cached_bounds.data()+0x20,disabled.data(),sizeof(disabled));
        assert(link.refresh_popup()&&link.popup_bounds_valid&&link.popup_scissor==disabled&&link.popup_viewport==viewport);
        for(const std::array<std::int32_t,4> bad:{std::array<std::int32_t,4>{1,0,maximum,maximum},
                {0,-1,maximum,maximum},{0,0,maximum,720},{0,0,1280,maximum},{0,0,maximum-1,maximum},{0,0,65537,720}}){
            std::memcpy(cached_bounds.data()+0x20,bad.data(),sizeof(bad));
            assert(link.refresh_popup()&&!link.popup_bounds_valid);
        }
        std::memcpy(cached_bounds.data()+0x20,scissor.data(),sizeof(scissor));
        // Optional native observations must never make the existing popup read fail.
        fail_read_address=gfx_address+0xF80;
        assert(link.refresh_popup()&&link.popup_diagnostic==0xFF150503&&!link.popup_bounds_valid);
        assert((link.popup_viewport==std::array<float,4>{}));
        fail_read_address=main_base+gfx_pointer_offset;
        assert(link.refresh_popup()&&!link.popup_bounds_valid);
        fail_read_address=0;
        for(u64 invalid:{u64(0),u64(1),~u64(0),~u64(0)-0xFF}){
            gfx_pointer=invalid;const auto old_reads=native_reads;
            assert(link.refresh_popup()&&!link.popup_bounds_valid&&native_reads==old_reads+1);
        }
        gfx_pointer=gfx_address;
        for(unsigned invalid=0;invalid<6;++invalid){
            auto bad=viewport;
            bad[invalid<2?0:2]=invalid==0?std::numeric_limits<float>::quiet_NaN():
                invalid==1?std::numeric_limits<float>::infinity():invalid==2?65537:
                invalid==3?-65537:invalid==4?-std::numeric_limits<float>::max():std::numeric_limits<float>::max();
            std::memcpy(cached_bounds.data(),bad.data(),sizeof(bad));
            assert(link.refresh_popup()&&!link.popup_bounds_valid);
        }
        std::memcpy(cached_bounds.data(),viewport.data(),sizeof(viewport));
        for(int invalid:{-65537,65537,std::numeric_limits<int>::min()}){
            auto bad=scissor;bad[2]=invalid;
            std::memcpy(cached_bounds.data()+0x20,bad.data(),sizeof(bad));
            assert(link.refresh_popup()&&!link.popup_bounds_valid);
        }
        auto empty=scissor;empty[2]=empty[3]=0;
        std::memcpy(cached_bounds.data()+0x20,empty.data(),sizeof(empty));
        assert(link.refresh_popup()&&link.popup_bounds_valid&&link.popup_scissor==empty);
        for(int observed:{0,-8,32768}){
            auto v=viewport;auto s=scissor;v[2]=float(observed);s[2]=observed;
            std::memcpy(cached_bounds.data(),v.data(),sizeof(v));std::memcpy(cached_bounds.data()+0x20,s.data(),sizeof(s));
            assert(link.refresh_popup()&&link.popup_bounds_valid&&link.popup_viewport==v&&link.popup_scissor==s);
        }
        for(unsigned invalid=0;invalid<5;++invalid){
            main_base=invalid==0?0:invalid==3?~u64(0)-0x1FFF:invalid==4?1:u64(0x7100000000);
            main_size=invalid==1?0:invalid==2?gfx_pointer_offset+7:0x1200000;
            const auto old_reads=native_reads;
            assert(link.refresh_popup()&&!link.popup_bounds_valid&&native_reads==old_reads);
        }
        main_base=0x7100000000;main_size=gfx_pointer_offset+8;
        assert(link.refresh_popup()&&link.popup_bounds_valid);
        build_supported=false;const auto old_reads=native_reads;
        assert(!link.refresh_popup()&&!link.popup_bounds_valid&&link.popup_diagnostic==0&&native_reads==old_reads);
        link.close();assert(!link.popup_bounds_valid);
    }
    {
        reset();GameLink link;link.initialize();assert(link.connect());
        auto block=snapshot();block.graphics_reserved=0x07030803;std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(link.refresh_popup()&&link.popup_diagnostic==0x07030803);
        ++pid;assert(!link.refresh_popup()&&link.popup_diagnostic==0);link.close();
    }
    {
        reset();GameLink link;link.initialize();assert(link.connect());
        for(unsigned status=0;status<4;++status){
            auto block=snapshot();block.appearance_applied=unsigned(re4::CharacterAppearance::Ashley);
            block.appearance_status=status;block.appearance_error=7;block.appearance_generation=2*(status+1);
            std::memcpy(memory.data()+8,&block,sizeof(block));
            assert(link.refresh_appearance()&&link.appearance.valid);
            assert(link.appearance.applied==re4::CharacterAppearance::Ashley&&unsigned(link.appearance.status)==status);
            assert(link.appearance.error==7&&link.appearance.generation==2*(status+1));
        }
        auto block=snapshot();block.appearance_generation|=1;std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(!link.refresh_appearance()&&!link.appearance.valid);
        block.appearance_generation=12;std::memcpy(memory.data()+8,&block,sizeof(block));appearance_race=true;
        assert(!link.refresh_appearance()&&!link.appearance.valid);appearance_race=false;
        for(unsigned invalid=0;invalid<2;++invalid){
            block=snapshot();block.appearance_applied=invalid?0:8;block.appearance_status=invalid?4:0;
            std::memcpy(memory.data()+8,&block,sizeof(block));assert(!link.refresh_appearance()&&!link.appearance.valid);
        }
        ++pid;assert(!link.refresh_appearance()&&!link.appearance.valid);link.close();
    }
    reset();
    {
        GameLink link;link.initialize();assert(link.connect());
        assert(link.details.find("fps=7 reticle=9 move=11 pose=13 status=3 distance=12.500")!=std::string::npos);
        re4::Settings s;s.turn_gain=0.3f;assert(link.apply(s));
        const auto block=snapshot();assert(block.sequence==6);
        assert((write_sizes==std::vector<std::size_t>{4,re4::kSettingsBytes,4}));
        re4::Settings decoded;assert(re4::decode_settings(block.settings,sizeof(block.settings),decoded));
        assert(std::fabs(decoded.turn_gain-0.3f)<0.0001f);
        ++pid;assert(!link.apply(s) && write_count==3);link.close();
    }
    for(unsigned failure=1;failure<=3;++failure){
        reset();GameLink link;link.initialize();assert(link.connect());fail_write=failure;
        assert(!link.apply({}));assert(write_count==failure);
        assert(failure==1 ? snapshot().sequence==4 : (snapshot().sequence&1));
        link.close();
    }
    {
        reset();title=1;GameLink link;link.initialize();assert(!link.connect() && reads==0);link.close();
        reset();build_supported=false;link.initialize();assert(!link.connect() && reads==0);link.close();
        reset();attach_rc=7;link.initialize();assert(!link.connect() && reads==0);link.close();
        reset();mapping_count=33;link.initialize();assert(!link.connect() && reads==0);link.close();
        reset();memory[8]=0;link.initialize();assert(!link.connect() && write_count==0);link.close();
        reset();auto old=snapshot();old.abi=re4::kSharedAbi-1;
        std::memcpy(memory.data()+8,&old,sizeof(old));
        link.initialize();assert(!link.connect() && write_count==0);link.close();
        reset();old=snapshot();std::memcpy(old.marker,"RE4CTL_SHARED17",16);
        std::memcpy(memory.data()+8,&old,sizeof(old));
        link.initialize();assert(!link.connect() && write_count==0);link.close();
        reset();old=snapshot();std::memcpy(old.marker,"RE4CTL_SHARED20",16);
        std::memcpy(memory.data()+8,&old,sizeof(old));
        link.initialize();assert(!link.connect() && write_count==0);link.close();
        reset();old=snapshot();std::memcpy(old.marker,"RE4CTL_SHARED21",16);old.abi=21;
        std::memcpy(memory.data()+8,&old,sizeof(old));
        link.initialize();assert(!link.connect() && write_count==0);link.close();
    }
    std::puts("actual game IPC: identity, mapping bounds, odd/payload/even and restart protection PASS");
}
