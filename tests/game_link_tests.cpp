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
bool module_frame_race=false;bool synthetic_scan_pages=false;bool module_candidate_race=false;unsigned metadata_fail_at=0,metadata_build_change_at=0;
u64 mapping_error_at=UINT64_MAX;bool mapping_error_bad_count=false;
bool build_supported=true;
bool appearance_race=false;
bool capture_race=false,settings_race=false;
unsigned emitter_race=0,emitter_reads=0;
unsigned hp_capture_race=0,hp_capture_reads=0;
constexpr u64 hp_capture_address=address+8+864,hp_capture_sequence=hp_capture_address+24;
constexpr u64 emitter_address=address+8+512,emitter_sequence=emitter_address+24;
constexpr u64 gfx_pointer_offset=0x1180D40+0x140,gfx_address=0x600000;
u64 main_base=0,main_size=0,gfx_pointer=0,fail_read_address=0;
std::array<u8,0x30> cached_bounds{};
unsigned native_reads=0,health_ipc_reads=0,health_metadata_calls=0,health_metadata_change_at=0;
u64 health_race_address=0;unsigned health_race_reads=0;
std::vector<std::size_t> write_sizes;
struct Segment {u64 address;std::vector<u8> bytes;};
std::vector<Segment> segments;
std::vector<MemoryInfo> diagnostic_mappings;
std::vector<u64> diagnostic_page_reads;
template<class T>void segment(u64 at,const T& value){
    Segment s{at,std::vector<u8>(sizeof(T))};std::memcpy(s.bytes.data(),&value,sizeof(T));segments.push_back(s);
}
re4::SharedControl snapshot(){
    re4::SharedControl block{};std::memcpy(&block,memory.data()+8,sizeof(block));return block;
}
void reset(){
    mapping_error_at=UINT64_MAX;mapping_error_bad_count=false;
    module_frame_race=false;synthetic_scan_pages=false;module_candidate_race=false;metadata_fail_at=metadata_build_change_at=0;
    memory.fill(0);pid=42;title=re4::profile::title_id;mapping_count=1;
    attach_rc=attach_calls=write_count=fail_write=reads=0;build_supported=true;appearance_race=false;write_sizes.clear();
    main_base=main_size=gfx_pointer=fail_read_address=0;cached_bounds.fill(0);native_reads=0;
    capture_race=settings_race=false;segments.clear();diagnostic_mappings.clear();diagnostic_page_reads.clear();
    hp_capture_race=hp_capture_reads=0;emitter_race=emitter_reads=0;health_ipc_reads=health_metadata_calls=health_metadata_change_at=health_race_reads=0;health_race_address=0;
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
        ++health_metadata_calls;if(metadata_fail_at&&health_metadata_calls>=metadata_fail_at)return 9;
        auto* m=static_cast<Metadata*>(out);*m={};m->pid=pid;m->title=title;
        m->main={main_base,main_size};
        std::memcpy(m->build_id,re4::profile::build_id,32);if(!build_supported||(metadata_build_change_at&&health_metadata_calls>=metadata_build_change_at))m->build_id[0]^=1;
        if(health_metadata_change_at&&health_metadata_calls>=health_metadata_change_at)++m->pid;
        return 0;
    }
    if(command==65101){
        const u64 offset=*static_cast<const u64*>(in);
        if(offset==mapping_error_at){*static_cast<u64*>(out)=mapping_error_bad_count?33:0;return mapping_error_bad_count?0:9;}
        if(!diagnostic_mappings.empty()){
            const auto count=offset<diagnostic_mappings.size()?std::min<u64>(32,diagnostic_mappings.size()-offset):0;
            *static_cast<u64*>(out)=count;
            auto* mappings=static_cast<MemoryInfo*>(const_cast<void*>(p.buffers[0].data));
            for(u64 i=0;i<count;++i)mappings[i]=diagnostic_mappings[offset+i];
            return 0;
        }
        *static_cast<u64*>(out)=offset?0:mapping_count;
        auto* mappings=static_cast<MemoryInfo*>(const_cast<void*>(p.buffers[0].data));
        mappings[0]={address,memory.size(),MemType_CodeMutable,Perm_Rw};
        if(synthetic_scan_pages)for(unsigned i=0;i<4;++i)mappings[i]={address+u64(i)*0x20000,0x20000,MemType_CodeMutable,Perm_Rw};
        return 0;
    }
    assert(command==65102||command==65103);
    struct Range{u64 addr,size;};const auto r=*static_cast<const Range*>(in);
    if(command==65102){
        ++health_ipc_reads;
        if(!(r.addr&0xFFF))for(const auto& mapping:diagnostic_mappings)
            if(r.addr>=mapping.addr&&r.addr-mapping.addr<mapping.size){diagnostic_page_reads.push_back(r.addr);break;}
        if(synthetic_scan_pages&&r.addr>=address&&r.addr-address<0x80000){std::memset(const_cast<void*>(p.buffers[0].data),0,r.size);return 0;}
        if(r.addr==health_race_address&&++health_race_reads>1){std::memset(const_cast<void*>(p.buffers[0].data),0,r.size);return 0;}
        if(r.addr==fail_read_address)return 9;
        for(const auto& s:segments)if(r.addr>=s.address&&r.addr-s.address<=s.bytes.size()&&r.size<=s.bytes.size()-(r.addr-s.address)){
            std::memcpy(const_cast<void*>(p.buffers[0].data),s.bytes.data()+r.addr-s.address,r.size);return 0;
        }
        for(const auto& mapping:diagnostic_mappings)if(r.addr>=mapping.addr&&r.addr-mapping.addr<=mapping.size&&r.size<=mapping.size-(r.addr-mapping.addr)){
            auto* output=static_cast<u8*>(const_cast<void*>(p.buffers[0].data));std::memset(output,0,r.size);
            const auto copy=[&](u64 at,const u8* bytes,std::size_t length){
                const u64 first=std::max(r.addr,at),last=std::min(r.addr+r.size,at+length);
                if(first<last)std::memcpy(output+first-r.addr,bytes+first-at,std::size_t(last-first));
            };
            if(mapping.addr==address)copy(address,memory.data(),memory.size());
            for(const auto& s:segments)copy(s.address,s.bytes.data(),s.bytes.size());
            return 0;
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
        if(r.addr>=emitter_address&&r.addr<emitter_address+352){
            ++emitter_reads;assert(r.size<=352);
            if(emitter_race==1&&r.addr==emitter_sequence){
                u32 value=0;std::memcpy(&value,memory.data()+emitter_sequence-address,4);value+=2;
                std::memcpy(memory.data()+emitter_sequence-address,&value,4);
            }
            if(emitter_race==2&&r.addr==emitter_address&&r.size==352){
                auto* copy=static_cast<re4::EmitterDiagnosticsWire*>(const_cast<void*>(p.buffers[0].data));copy->sequence+=2;
            }
            if(emitter_race==3&&r.addr==emitter_address&&r.size==352){
                u32 value=0;std::memcpy(&value,memory.data()+emitter_sequence-address,4);value+=2;
                std::memcpy(memory.data()+emitter_sequence-address,&value,4);
            }
        }
        if(r.addr>=hp_capture_address&&r.addr<hp_capture_address+160){
            ++hp_capture_reads;assert(r.size<=160);
            if(hp_capture_race==1&&r.addr==hp_capture_sequence){
                u32 value=0;std::memcpy(&value,memory.data()+hp_capture_sequence-address,4);value+=2;
                std::memcpy(memory.data()+hp_capture_sequence-address,&value,4);
            }
            if(hp_capture_race==3&&hp_capture_reads==1&&r.addr==hp_capture_sequence){
                u32 value=0;std::memcpy(&value,memory.data()+hp_capture_sequence-address,4);value+=2;
                std::memcpy(memory.data()+hp_capture_sequence-address,&value,4);
            }
            if(hp_capture_race==2&&r.addr==hp_capture_address&&r.size==160){
                auto* bytes=static_cast<u8*>(const_cast<void*>(p.buffers[0].data));u32 value=0;
                std::memcpy(&value,bytes+24,4);value+=2;std::memcpy(bytes+24,&value,4);
            }
        }
        if(module_frame_race&&r.addr==address){auto candidate=snapshot();++candidate.frames;std::memcpy(memory.data()+8,&candidate,sizeof(candidate));}
        if(module_candidate_race&&r.addr==address){auto candidate=snapshot();candidate.status=0;std::memcpy(memory.data()+8,&candidate,sizeof(candidate));}
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
        // Console19 shape: four earlier large writable regions consume a depth-first budget.
        reset();auto block=snapshot();std::memcpy(block.module_version,"0.28.19",8);block.frames=919;
        std::memcpy(memory.data()+8,&block,sizeof(block));
        for(unsigned i=0;i<4;++i)diagnostic_mappings.push_back({0x10000+u64(i)*0x20000,0x20000,MemType_CodeMutable,Perm_Rw});
        diagnostic_mappings.push_back({address,memory.size(),MemType_CodeMutable,Perm_Rw});
        GameLink client;client.initialize();re4::LiveDiagnosticSnapshot live;
        assert(client.read_diagnostic_snapshot(live)&&live.connected&&live.status==re4::LiveDiagnosticStatus::Connected);
        assert(live.module_observation.found&&live.module_observation.stable&&live.module_observation.frames==919);
        assert(live.module_observation.scan_reads<=6&&!live.module_observation.scan_limit&&live.module_observation.scan_bytes<=512*1024);
        assert(diagnostic_page_reads.size()==5&&diagnostic_page_reads[4]==address&&write_count==0);
        client.close();
    }

    {
        // Page0 fairness also crosses the32-entry mapping enumeration boundary.
        reset();auto block=snapshot();std::memcpy(block.module_version,"0.28.19",8);block.frames=932;
        for(unsigned i=0;i<40;++i)diagnostic_mappings.push_back({0x1000000+u64(i)*0x20000,0x20000,MemType_CodeMutable,Perm_Rw});
        constexpr u64 remote_module=0x2000000;diagnostic_mappings.push_back({remote_module,4096,MemType_CodeMutable,Perm_Rw});
        segments.push_back({remote_module,std::vector<u8>(4096)});std::memcpy(segments.back().bytes.data()+8,&block,sizeof(block));
        GameLink batched;batched.initialize();re4::LiveDiagnosticSnapshot live;
        assert(batched.read_diagnostic_snapshot(live)&&live.connected&&live.module_observation.frames==932);
        assert(live.module_observation.scan_reads==42&&diagnostic_page_reads.size()==41&&diagnostic_page_reads.back()==remote_module);
        assert(live.module_observation.scan_bytes==40*4600+4096+sizeof(re4::SharedControl));batched.close();
        // A failed early page must not prevent discovery on a later mapping's first page.
        reset();block=snapshot();std::memcpy(block.module_version,"0.28.19",8);std::memcpy(memory.data()+8,&block,sizeof(block));
        diagnostic_mappings={{0x10000,0x20000,MemType_CodeMutable,Perm_Rw},{address,memory.size(),MemType_CodeMutable,Perm_Rw}};
        fail_read_address=0x10000;GameLink skipped;skipped.initialize();
        assert(skipped.read_diagnostic_snapshot(live)&&live.connected&&live.module_observation.scan_reads==3);skipped.close();
        // Older off-page blocks still work, with deeper pages visited round-robin.
        reset();block=snapshot();std::memcpy(block.module_version,"0.28.17",8);memory.fill(0);
        diagnostic_mappings={{0x10000,0x4000,MemType_CodeMutable,Perm_Rw},{address,0x4000,MemType_CodeMutable,Perm_Rw}};
        segments.push_back({address+0x2000,std::vector<u8>(0x2000)});std::memcpy(segments.back().bytes.data()+8,&block,sizeof(block));
        GameLink legacy;legacy.initialize();assert(legacy.read_diagnostic_snapshot(live)&&live.connected);
        const std::vector<u64> expected_pages={0x10000,address,0x11000,address+0x1000,0x12000,address+0x2000};
        assert(diagnostic_page_reads==expected_pages&&live.module_observation.scan_reads==7);legacy.close();
        // Failed/truncated enumeration is incomplete coverage, not a missing module claim.
        for(bool bad_count:{false,true}){
            reset();for(unsigned i=0;i<32;++i)diagnostic_mappings.push_back({0x1000000+u64(i)*4096,4096,MemType_CodeMutable,Perm_Rw});
            mapping_error_at=32;mapping_error_bad_count=bad_count;GameLink enumeration;enumeration.initialize();
            assert(!enumeration.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::DiscoveryLimited);
            assert(live.module_observation.scan_limit&&!live.module_observation.found&&live.module_observation.scan_reads==32);enumeration.close();
        }
        reset();diagnostic_mappings={{0x10000,4096,MemType_CodeMutable,Perm_Rw}};fail_read_address=0x10000;
        GameLink unread;unread.initialize();assert(!unread.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::DiscoveryLimited);unread.close();
        // Last scheduled read loses176bytes to budget; absence must still be limited.
        reset();for(unsigned i=0;i<10;++i)diagnostic_mappings.push_back({0x1000000+u64(i)*0x20000,u64(i<5?12:11)*4096,MemType_CodeMutable,Perm_Rw});
        GameLink partial;partial.initialize();
        assert(!partial.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::DiscoveryLimited);
        assert(live.module_observation.scan_limit&&live.module_observation.scan_reads==115);
        assert(live.module_observation.scan_bytes==512*1024-sizeof(re4::SharedControl));partial.close();
    }
    {
        // Known21 native observations preserve diagnosis after an application update.
        re4::LiveDiagnosticSnapshot live;
        for(const char* version:{"0.28.20","0.28.21","0.28.22","0.28.23","0.28.24","0.28.25","0.28.26","0.28.27","0.28.28","0.28.29"}){
            reset();auto block=snapshot();block.status=2;std::memcpy(block.module_version,version,8);
            std::memcpy(memory.data()+8,&block,sizeof(block));GameLink current;current.initialize();
            assert(!current.read_diagnostic_snapshot(live)&&live.module_observation.found&&live.module_observation.stable);
            assert(std::string(live.module_observation.version.data())==version&&live.profile_observation.attempted);current.close();
        }
        reset();auto block=snapshot();std::memcpy(block.module_version,"0.28.30",8);std::memcpy(memory.data()+8,&block,sizeof(block));
        GameLink future;future.initialize();assert(future.connect());
        assert(future.read_diagnostic_snapshot(live)&&live.connected&&!live.module_observation.found&&!live.profile_observation.attempted);future.close();
    }
    {
        reset();GameLink client;client.initialize();auto failed=snapshot();failed.status=2;failed.frames=0;
        std::memcpy(failed.module_version,"0.28.18",8);std::memcpy(memory.data()+8,&failed,sizeof(failed));
        re4::LiveDiagnosticSnapshot live;
        assert(!client.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::ModuleMissing&&!live.connected);
        assert(live.module_observation.found&&live.module_observation.stable&&live.module_observation.status==2);
        assert(live.module_observation.abi==24&&live.module_observation.capabilities==re4::kCapabilityTurning);
        assert(std::string(live.module_observation.version.data())=="0.28.18");
        re4::Settings settings;assert(!client.apply(settings)&&write_count==0);
        client.close();
    }
    {
        const auto failed_fixture=[](){
            reset();main_base=0x10000000;main_size=0x1281000;
            auto block=snapshot();block.status=2;std::memcpy(block.module_version,"0.28.18",8);
            std::memcpy(memory.data()+8,&block,sizeof(block));
            const auto page_for=[](u32 rva)->Segment&{
                const u64 at=main_base+(rva&~u32(0xFFF));
                for(auto& page:segments)if(page.address==at)return page;
                segments.push_back({at,std::vector<u8>(4096)});return segments.back();
            };
            for(const auto& word:re4::health_profile::kAddedWords){auto& page=page_for(word.rva);std::memcpy(page.bytes.data()+(word.rva&0xFFF),&word.expected,4);}
            for(const auto& pointer:re4::health_profile::kAddedPointers){
                auto& page=page_for(pointer.rva);const u64 value=re4::health_profile::legacy18_invalid_null_pointer(pointer.rva)?0:main_base+pointer.expected_rva;
                std::memcpy(page.bytes.data()+(pointer.rva&0xFFF),&value,8);
            }
        };
        failed_fixture();GameLink client;client.initialize();re4::LiveDiagnosticSnapshot live;
        const auto native=segments;
        assert(!client.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::ModuleMissing);
        const auto& probe=live.profile_observation;
        assert(live.module_observation.found&&live.module_observation.stable&&live.module_observation.status==2);
        assert(probe.attempted&&probe.stable&&probe.complete&&!probe.budget_exceeded);
        assert(probe.words_checked==re4::health_profile::kAddedWordCount&&probe.pointers_checked==re4::health_profile::kAddedPointerCount);
        assert(probe.read_calls==segments.size()&&probe.read_calls<=128&&probe.pages<=128&&probe.bytes<=512*1024);
        assert(probe.mismatch_count==3&&probe.record_count==3);
        for(unsigned i=0;i<3;++i){const auto& m=probe.mismatches[i];
            assert(m.pointer&&m.rva==0xAFE588+8*i&&m.expected==0&&m.readable&&m.in_bounds&&m.observed_null&&!m.observed_in_main);}
        for(unsigned i=0;i<segments.size();++i)assert(segments[i].bytes==native[i].bytes);
        assert(write_count==0);
        client.close();
        // Healthy19 still reports18's historicalNULLrequirements as diagnostics only.
        failed_fixture();auto block=snapshot();block.status=1;block.frames=100;std::memcpy(block.module_version,"0.28.19",8);
        std::memcpy(memory.data()+8,&block,sizeof(block));GameLink healthy;healthy.initialize();
        assert(healthy.read_diagnostic_snapshot(live)&&live.connected&&live.profile_observation.mismatch_count==3);
        healthy.close();
        failed_fixture();block=snapshot();block.status=1;block.frames=100;std::memcpy(block.module_version,"0.28.19",8);
        std::memcpy(memory.data()+8,&block,sizeof(block));module_frame_race=true;GameLink ticking;ticking.initialize();
        assert(ticking.read_diagnostic_snapshot(live)&&live.module_observation.stable&&live.module_observation.frames==101);ticking.close();
        failed_fixture();block=snapshot();block.status=0;std::memcpy(block.module_version,"0.28.13",8);
        std::memcpy(memory.data()+8,&block,sizeof(block));block.status=2;std::memcpy(block.module_version,"0.28.18",8);
        std::memcpy(memory.data()+512,&block,sizeof(block));GameLink first;first.initialize();
        assert(!first.read_diagnostic_snapshot(live)&&live.module_observation.found&&live.module_observation.status==0);
        assert(std::string(live.module_observation.version.data())=="0.28.13"&&!live.profile_observation.attempted);first.close();

        // Old known versions have a strict observation, with no new guard probe.
        for(const char* version:{"0.28.13","0.28.17"}){failed_fixture();block=snapshot();std::memcpy(block.module_version,version,8);
            std::memcpy(memory.data()+8,&block,sizeof(block));GameLink old;old.initialize();
            assert(!old.read_diagnostic_snapshot(live)&&live.module_observation.found&&live.module_observation.stable&&!live.profile_observation.attempted);old.close();}
        failed_fixture();block=snapshot();std::memcpy(block.module_version,"0.28.13",8);std::memcpy(memory.data()+8,&block,sizeof(block));
        health_metadata_change_at=3;GameLink old_stale;old_stale.initialize();
        assert(!old_stale.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::ReadFailed&&!live.module_observation.found);old_stale.close();
        for(unsigned invalid=0;invalid<6;++invalid){failed_fixture();block=snapshot();
            if(invalid==0)block.abi=23;
            if(invalid==1)block.capabilities=0;
            if(invalid==2)std::memcpy(block.module_version,"0.28.30",8);
            if(invalid==3)std::memcpy(block.module_version,"0.28.018",9);
            if(invalid==4)block.status=3;
            if(invalid==5)block.marker[0]^=1;
            std::memcpy(memory.data()+8,&block,sizeof(block));GameLink invalid_client;invalid_client.initialize();
            assert(!invalid_client.read_diagnostic_snapshot(live)&&!live.module_observation.found&&!live.profile_observation.attempted);invalid_client.close();
        }
        for(unsigned mode=0;mode<4;++mode){failed_fixture();
            if(mode==0)health_metadata_change_at=3;
            if(mode==1)metadata_fail_at=3;
            if(mode==2)metadata_build_change_at=3;
            if(mode==3)health_metadata_change_at=2;
            GameLink stale;stale.initialize();assert(!stale.read_diagnostic_snapshot(live)&&live.status==re4::LiveDiagnosticStatus::ReadFailed);
            assert(!live.module_observation.found&&!live.profile_observation.stable&&live.profile_observation.record_count==0);stale.close();
        }
        failed_fixture();module_candidate_race=true;GameLink torn;torn.initialize();
        assert(!torn.read_diagnostic_snapshot(live)&&live.module_observation.found&&!live.module_observation.stable&&!live.profile_observation.attempted);torn.close();
        failed_fixture();fail_read_address=main_base+(re4::health_profile::kAddedWords[0].rva&~u32(0xFFF));
        GameLink unread;unread.initialize();assert(!unread.read_diagnostic_snapshot(live));
        assert(live.profile_observation.attempted&&live.profile_observation.stable&&!live.profile_observation.complete&&live.profile_observation.unread_pages==1);
        assert(live.profile_observation.record_count<=8&&live.profile_observation.read_calls<=128);unread.close();
        failed_fixture();
        for(unsigned i=0;i<10;++i){const auto& word=re4::health_profile::kAddedWords[i];
            for(auto& page:segments)if(page.address==main_base+(word.rva&~u32(0xFFF))){const u32 actual=word.expected^1;std::memcpy(page.bytes.data()+(word.rva&0xFFF),&actual,4);}}
        GameLink many;many.initialize();assert(!many.read_diagnostic_snapshot(live));
        assert(live.profile_observation.complete&&live.profile_observation.mismatch_count==13&&live.profile_observation.record_count==8);
        assert(live.profile_observation.mismatches[0].rva==re4::health_profile::kAddedWords[0].rva);many.close();
        failed_fixture();
        const auto pointer=re4::health_profile::kAddedPointers[0];
        for(auto& page:segments)if(page.address==main_base+(pointer.rva&~u32(0xFFF))){const u64 outside=UINT64_MAX;std::memcpy(page.bytes.data()+(pointer.rva&0xFFF),&outside,8);}
        GameLink outside;outside.initialize();assert(!outside.read_diagnostic_snapshot(live));
        bool outside_record=false;for(unsigned i=0;i<live.profile_observation.record_count;++i){const auto& m=live.profile_observation.mismatches[i];
            if(m.rva==pointer.rva){outside_record=true;assert(m.pointer&&m.readable&&!m.observed_in_main&&!m.observed_null);}}
        assert(outside_record);outside.close();
        reset();synthetic_scan_pages=true;mapping_count=4;GameLink limited;limited.initialize();
        assert(!limited.read_diagnostic_snapshot(live)&&live.module_observation.scan_limit&&!live.module_observation.found&&live.status==re4::LiveDiagnosticStatus::DiscoveryLimited);
        assert(live.module_observation.scan_reads<=128&&live.module_observation.scan_bytes<=512*1024&&!live.profile_observation.attempted);limited.close();
        reset();std::memset(memory.data()+8,0,sizeof(re4::SharedControl));GameLink missing;missing.initialize();
        assert(!missing.read_diagnostic_snapshot(live)&&!live.module_observation.found&&!live.profile_observation.attempted);missing.close();
    }
    // A live qualified target must yield copied native context and applied settings without any IPC writes.
    {
        reset();main_base=0x10000000;main_size=0x1281000;
        constexpr u64 actor=0x20000000,state=0x20010000,context=0x20020000,weapon=0x20030000,pool=0x20100000,target=pool+0x10C8;
        segment(main_base+re4::profile::player_slot,actor);segment(main_base+0xB1AE08,state);
        segment(actor+0x938,context);segment(context+0x38,weapon);segment(weapon+0x418,target);
        segment(actor+0x114,std::uint32_t(0x00010600));segment(main_base+0x1131718+0x48,std::uint8_t(1));segment(main_base+0x1131718+0x54,std::uint8_t(10));
        segment(main_base+re4::profile::input_flags,std::uint64_t(0x10));
        struct PoolInfo{u64 base;u32 count,stride;};segment(main_base+0xAE6F00,PoolInfo{pool,3,0x10C8});
        segment(state+0x5034,std::uint8_t(2));segment(state+0x84F0,std::uint8_t(6));segment(state+0x500C,std::uint8_t(10));
        segment(target,main_base+0xAE7050);segment(main_base+0xAE7078,main_base+0x8F470);
        segment(target+8,std::uint32_t(1));segment(target+0x118,std::uint16_t(0x0010));
        segment(target+0x3C2,std::uint32_t(0x05DC05DC));segment(target+0x48D,std::uint8_t(2));
        segment(target+0xB8C,std::uint8_t(2));segment(target+0xB4C,std::uint8_t(0));
        segment(target+0x944,std::uint64_t(0));segment(target+0x974,std::uint64_t(0));segment(main_base+0xAF0564,std::int32_t(150));
        auto block=snapshot();re4::Settings requested;requested.enemy_health_bars=true;requested.enemy_health_numbers=true;
        assert(re4::encode_settings(requested,block.settings,sizeof(block.settings)));block.applied_sequence=block.sequence;block.settings_source=4;
        std::memcpy(memory.data()+8,&block,sizeof(block));const auto unchanged=memory;const auto native=segments;
        GameLink link;link.initialize();assert(link.connect());re4::HealthReport report;
        const auto sample=[&](){re4::LiveDiagnosticSnapshot live;const bool connected=link.read_diagnostic_snapshot(live);report=live.health_reference;return connected&&report.status==re4::HealthReportStatus::Collected;};
        assert(sample());
        assert(report.status==re4::HealthReportStatus::Collected && report.predicted_unit==58);
        assert(report.numbers_on&&report.bars_on&&report.settings_applied&&report.observed_consistent);
        assert(report.vptr_rva==0xAE7050&&report.update_rva==0x8F470&&report.target_slot==1);
        assert(report.type==0x10&&report.part_flags==2&&report.current==1500&&report.coefficient==150);
        assert(report.rejected_mask==0&&report.first_failed_gate==re4::HealthReportGate::None&&report.ipc_reads<=100);
        assert(write_count==0&&memory==unchanged&&segments.size()==native.size());
        for(unsigned i=0;i<segments.size();++i)assert(segments[i].address==native[i].address&&segments[i].bytes==native[i].bytes);
        const auto put=[&](u64 at,const auto& value){for(auto& seg:segments)if(seg.address==at){assert(seg.bytes.size()==sizeof(value));std::memcpy(seg.bytes.data(),&value,sizeof(value));return;}segment(at,value);};
        // Exact Common handler keeps Common arithmetic even when actual118 overlaps a module label.
        for(unsigned type:{0x31u,0x2Bu,0x2Fu,0x32u,0x3Fu}){
            put(target+0x118,std::uint16_t(type));put(main_base+0xAF1440+u64(type-0x23)*8,main_base+0xAF055C);
            assert(sample()&&report.type==type&&report.coefficient==150);
            assert(report.predicted_unit==58&&report.first_failed_gate==re4::HealthReportGate::None&&report.rejected_mask==0);
        }
        segments=native;
        // A distinct authenticated Plain family must use its own body path, not Common B8C math.
        put(target,main_base+0xAE8170);put(main_base+0xAE8198,main_base+0xDF550);
        put(target+0x118,std::uint16_t(0x0622));
        assert(sample()&&report.type==0x22&&report.subtype==6&&report.vptr_rva==0xAE8170&&report.update_rva==0xDF550);
        assert(report.first_failed_gate==re4::HealthReportGate::None&&report.predicted_unit==127&&report.rejected_mask==0);
        put(target+0xB8C,std::uint8_t(255));
        for(u64 unavailable:{target+0x48D,target+0xB8C,target+0xB4C,target+0x944,target+0x974}){
            fail_read_address=unavailable;
            assert(sample()&&report.predicted_unit==127&&report.first_failed_gate==re4::HealthReportGate::None);
            assert(report.ipc_reads<=100&&write_count==0);fail_read_address=0;
        }
        segments=native;
        // Independent native census: family identity is vptr/update; actual1181E selects table/rank.
        struct FamilyFixture {u32 vptr,update,unit;bool legacy;};
        const FamilyFixture family_fixtures[]={
            {0xae7050,0x8f470,58,false},
            {0xae7f00,0xd8a50,1500,false},
            {0xae8038,0xdc800,0,false},
            {0xae8170,0xdf550,127,false},
            {0xae8450,0xe6350,850,false},
            {0xae85b0,0xe87a0,1500,false},
            {0xae8668,0xe9cf0,255,false},
            {0xae8938,0xeda10,127,false},
            {0xae8a18,0xee9d0,850,false},
            {0xae8b10,0xf0bf0,1500,false},
            {0xae8c08,0xf2650,850,false},
            {0xae8d38,0xf50f0,1500,false},
            {0xae8e28,0xf7bf0,0,false},
            {0xae9438,0x106890,0,true},
            {0xae99d0,0x116230,127,false},
            {0xae9ea8,0x126010,1500,false},
            {0xae9f58,0x126fa0,0,false},
            {0xaea0e0,0x12b120,8,false},
            {0xaea170,0x12c0e0,0,true},
            {0xaea7e0,0x139ba0,127,false},
            {0xaeab58,0x147dc0,8,false},
            {0xaeafd0,0x148e20,0,true},
            {0xaeb618,0x154910,127,false},
            {0xaebc60,0x1629f0,0,true},
            {0xaebf38,0x16a5c0,0,true},
            {0xaec530,0x180eb0,637,false},
            {0xaec720,0x1855e0,170,false},
            {0xaec918,0x187780,127,false},
            {0xaeccd8,0x18d6d0,0,false},
            {0xaed668,0x1b3610,1,false},
            {0xaece20,0x18fed0,63,false},
            {0xaed178,0x1a2e20,127,false},
        };
        static_assert(std::size(family_fixtures)==32);
        for(const auto entry:family_fixtures){
            segments=native;put(target,main_base+entry.vptr);put(main_base+entry.vptr+0x28,main_base+entry.update);
            put(target+0x118,std::uint16_t(0x001E));
            if(entry.vptr!=0xAE7050){put(target+0xB8C,std::uint8_t(255));put(target+0xB4C,std::uint8_t(4));
                put(target+0x944,std::uint64_t(0xDEADBEEF));put(target+0x974,std::uint64_t(0xCAFE));}
            assert(sample()&&report.type==0x1E&&report.vptr_rva==entry.vptr&&report.update_rva==entry.update);
            assert(report.predicted_unit==entry.unit&&report.ipc_reads<=100&&write_count==0);
            const auto gate=entry.legacy?re4::HealthReportGate::UnsupportedFamily:entry.unit?re4::HealthReportGate::None:re4::HealthReportGate::Prediction;
            assert(report.first_failed_gate==gate);
            put(main_base+entry.vptr+0x28,main_base+entry.update+4);
            assert(sample()&&report.predicted_unit==0&&report.first_failed_gate==re4::HealthReportGate::Class);
        }
        // Unused body/coefficient/mode/rank reads do not make non-consuming families fail.
        for(u32 vptr:{0xAE7F00u,0xAE8038u,0xAED668u,0xAEA0E0u}){
            const u32 update=vptr==0xAE7F00?0xD8A50:vptr==0xAE8038?0xDC800:vptr==0xAED668?0x1B3610:0x12B120;
            const u32 expected=vptr==0xAE7F00?1500:vptr==0xAE8038?0:vptr==0xAED668?1:8;
            segments=native;put(target,main_base+vptr);put(main_base+vptr+0x28,main_base+update);
            put(target+0x118,std::uint16_t(0x001E));put(main_base+0xAF0564,std::int32_t(9999));
            put(state+0x84F0,std::uint8_t(0));
            if(vptr!=0xAEA0E0)put(state+0x500C,std::uint8_t(255));
            assert(sample()&&report.coefficient==9999&&report.predicted_unit==expected);
            for(u64 unavailable:{target+0xB8C,target+0xB4C,target+0x944,target+0x974,main_base+0xAF0564,state+0x84F0}){
                fail_read_address=unavailable;
                assert(sample()&&report.predicted_unit==expected);fail_read_address=0;
            }
            if(vptr!=0xAEA0E0){fail_read_address=state+0x500C;assert(sample()&&report.predicted_unit==expected);fail_read_address=0;}
            fail_read_address=state+0x5034;
            assert(!sample()&&report.status==re4::HealthReportStatus::ReadFailed&&report.first_failed_gate==re4::HealthReportGate::Weapon);
            fail_read_address=0;
        }
        segments=native;fail_read_address=target+0x48D;
        assert(sample()&&report.predicted_unit==58&&report.first_failed_gate==re4::HealthReportGate::None);fail_read_address=0;
        // Positive nativeC families still require their table observation; zeroC is definedimmune.
        put(target,main_base+0xAE8170);put(main_base+0xAE8198,main_base+0xDF550);
        put(target+0x118,std::uint16_t(0x001E));put(main_base+0xAF0564,std::int32_t(0));
        assert(sample()&&report.predicted_unit==0&&report.first_failed_gate==re4::HealthReportGate::Prediction);
        fail_read_address=main_base+0xAF0564;
        assert(!sample()&&report.status==re4::HealthReportStatus::ReadFailed&&report.first_failed_gate==re4::HealthReportGate::Coefficient);fail_read_address=0;
        segments=native;
        // Fixed harpoon handler admits native index21 and does not require a coefficient row.
        put(target,main_base+0xAE9F58);put(main_base+0xAE9F80,main_base+0x126FA0);
        put(target+0x118,std::uint16_t(0x002F));put(main_base+0xAF14A0,main_base+0xAF055C);
        put(state+0x5034,std::uint8_t(21));put(state+0x84F0,std::uint8_t(0));
        assert(sample()&&report.weapon_index==21&&report.type==0x2F&&report.predicted_unit==85);
        assert(!(report.read_mask&re4::HealthCoefficient)&&report.first_failed_gate==re4::HealthReportGate::None);
        segments=native;put(target,main_base+0xAE7F00);put(main_base+0xAE7F28,main_base+0xD8A50);
        put(target+0x3C2,std::uint32_t(0x05DC0064));
        assert(sample()&&report.current==100&&report.maximum==1500&&report.predicted_unit==1500);
        segments=native;put(target,main_base+0xAED668);put(main_base+0xAED690,main_base+0x1B3610);
        for(unsigned subtype:{0u,9u,10u,255u}){
            put(target+0x118,std::uint16_t((subtype<<8)|0x1E));
            assert(sample()&&report.predicted_unit==(subtype<10?1u:0u));
        }
        segments=native;put(target,main_base+0xAEC720);put(main_base+0xAEC748,main_base+0x1855E0);
        for(unsigned subtype:{1u,2u}){
            put(target+0x118,std::uint16_t((subtype<<8)|0x1E));
            assert(sample()&&report.predicted_unit==0&&report.first_failed_gate==re4::HealthReportGate::Prediction);
        }
        segments=native;put(target,main_base+0xAEC530);put(main_base+0xAEC558,main_base+0x180EB0);
        put(main_base+0xAF0564,std::int32_t(5000));
        assert(sample()&&report.coefficient==5000&&report.predicted_unit==21250&&report.first_failed_gate==re4::HealthReportGate::None);
        segments=native;
        // Console capture17: ordinary vptr/update, type1E/subtype17, Professional handgun body.
        put(target+0x118,std::uint16_t(0x171E));put(target+0xB4C,std::uint8_t(15));
        assert(sample()&&report.type==0x1E&&report.subtype==0x17&&report.phase==15);
        assert(report.current==1500&&report.maximum==1500&&report.body_status==2&&report.mode==6&&report.rank==10);
        assert(report.weapon_index==2&&report.coefficient==150&&!report.link944&&!report.link974);
        assert(report.first_failed_gate==re4::HealthReportGate::None&&report.rejected_mask==0&&report.predicted_unit==58);
        assert(report.vptr_rva==0xAE7050&&report.update_rva==0x8F470&&report.settings_applied&&write_count==0);
        for(unsigned subtype=0;subtype<256;++subtype){
            if(subtype==2||subtype==6||subtype==0xA||subtype==0xD)continue;
            put(target+0x118,std::uint16_t((subtype<<8)|0x1E));
            assert(sample()&&report.subtype==subtype&&report.first_failed_gate==re4::HealthReportGate::None);
            assert(report.rejected_mask==0&&report.predicted_unit==58&&report.mode==6&&report.rank==10);
        }
        // Audited selector0 special families use the same collected native scalars.
        struct Special {unsigned subtype,unlinked,linked;};
        for(const auto entry:{Special{2,39,27},Special{0xA,7,7},Special{0xD,2,2},Special{6,0,0}}){
            for(unsigned links=0;links<4;++links){
                put(target+0x118,std::uint16_t((entry.subtype<<8)|0x1E));
                put(target+0x944,std::uint64_t(links&1?0xDEADBEEF:0));put(target+0x974,std::uint64_t(links&2?0xCAFE:0));
                assert(sample()&&report.subtype==entry.subtype&&report.link944==bool(links&1)&&report.link974==bool(links&2));
                assert(report.predicted_unit==(links?entry.linked:entry.unlinked));
                assert(report.first_failed_gate==(entry.subtype==6?re4::HealthReportGate::Prediction:re4::HealthReportGate::None));
                assert(report.rejected_mask==(entry.subtype==6?re4::health_report_gate_bit(re4::HealthReportGate::Prediction):0));
            }
        }
        struct TmpSpecial {unsigned phase,unlinked,linked;};
        put(target+0x118,std::uint16_t(0x021E));put(state+0x5034,std::uint8_t(11));put(main_base+0xAF0588,std::int32_t(210));
        for(const auto entry:{TmpSpecial{15,27,18},TmpSpecial{4,13,9}}){
            put(target+0xB4C,std::uint8_t(entry.phase));
            for(unsigned links=0;links<2;++links){
                put(target+0x944,std::uint64_t(links?0xDEADBEEF:0));put(target+0x974,std::uint64_t(0));
                assert(sample()&&report.subtype==2&&report.weapon_index==11&&report.phase==entry.phase);
                assert(report.predicted_unit==(links?entry.linked:entry.unlinked)&&report.first_failed_gate==re4::HealthReportGate::None);
            }
        }
        segments=native;
        put(target+0x118,std::uint16_t(0x0110));put(target+0x944,std::uint64_t(0xDEADBEEF));
        assert(sample()&&report.subtype==1&&report.link944&&report.predicted_unit==39);
        put(target+0x974,std::uint64_t(0xCAFE));assert(sample()&&report.predicted_unit==39); // Both links scale once; neither is dereferenced.
        put(state+0x5034,std::uint8_t(11));put(main_base+0xAF0588,std::int32_t(210));put(target+0xB4C,std::uint8_t(4));
        assert(sample()&&report.weapon_index==11&&report.phase==4&&report.predicted_unit==27);
        segments=native;
        for(unsigned mode:{2u,3u,4u,5u,6u,7u,255u}){
            put(state+0x84F0,std::uint8_t(mode));put(state+0x500C,std::uint8_t(8));
            assert(sample()&&report.mode==mode&&report.rank==8&&report.predicted_unit==62&&report.predictor_version==18);
        }
        segments=native;
        for(unsigned mode:{0u,1u}){
            put(state+0x84F0,std::uint8_t(mode));
            assert(sample()&&report.mode==mode&&report.predicted_unit==0&&report.first_failed_gate==re4::HealthReportGate::ModeRank);
        }
        put(state+0x84F0,std::uint8_t(6));put(state+0x500C,std::uint8_t(0));
        assert(sample()&&report.mode==6&&report.rank==0&&report.predicted_unit==138);
        segments=native;
        put(state+0x5034,std::uint8_t(13));put(main_base+0xAF0590,std::int32_t(150));
        assert(sample()&&report.weapon_index==13&&report.coefficient==150&&report.predicted_unit==0&&report.first_failed_gate==re4::HealthReportGate::Weapon);
        segments=native;
        requested.enemy_health_numbers=false;assert(re4::encode_settings(requested,block.settings,sizeof(block.settings)));std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(sample()&&!report.numbers_on&&report.predicted_unit==58&&report.first_failed_gate==re4::HealthReportGate::NumbersOff);
        memory=unchanged;block=snapshot();block.applied_sequence=2;std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(sample()&&report.settings_valid&&!report.settings_applied&&report.first_failed_gate==re4::HealthReportGate::SettingsPending);
        memory=unchanged;settings_race=true;
        assert(sample()&&!report.settings_valid&&report.first_failed_gate==re4::HealthReportGate::SettingsRead);settings_race=false;memory=unchanged;
        put(target+0x118,std::uint16_t(0x0610));
        assert(sample()&&report.subtype==6&&report.predicted_unit==0&&report.first_failed_gate==re4::HealthReportGate::Prediction);segments=native;
        put(target+0x118,std::uint16_t(0x002C));put(main_base+0xAF1488,main_base+0xAF055C);
        put(target,main_base+0xAE9438);put(main_base+0xAE9460,main_base+0x106890);
        assert(sample()&&report.type==0x2C&&report.predicted_unit==0&&report.first_failed_gate==re4::HealthReportGate::UnsupportedFamily);segments=native;
        put(main_base+0xAF0564,std::int32_t(-1));
        assert(sample()&&report.coefficient==-1&&report.predicted_unit==0&&report.first_failed_gate==re4::HealthReportGate::Coefficient);segments=native;
        put(main_base+0xAE7078,main_base+0x8F474);
        assert(sample()&&report.update_rva==0x8F474&&report.first_failed_gate==re4::HealthReportGate::Class);segments=native;
        put(weapon+0x418,std::uint64_t(0));
        assert(!sample()&&report.status==re4::HealthReportStatus::NoTarget&&report.weapon_index==2&&report.mode==6&&report.routine_pack==0x00010600);
        assert((report.read_mask&re4::HealthGlobals)&&report.observed_consistent&&report.first_failed_gate==re4::HealthReportGate::Target);segments=native;
        put(main_base+0xAE6F00,PoolInfo{pool,3,0x10C9});
        assert(!sample()&&report.pool_stride==0x10C9&&report.first_failed_gate==re4::HealthReportGate::Pool);segments=native;
        fail_read_address=target+0xB8C;
        assert(!sample()&&report.status==re4::HealthReportStatus::ReadFailed&&report.type==0x10&&report.first_failed_gate==re4::HealthReportGate::BodyStatus);fail_read_address=0;
        health_race_address=weapon+0x418;
        assert(!sample()&&report.status==re4::HealthReportStatus::Unstable&&!report.observed_consistent&&report.predicted_unit==0);health_race_address=0;
        health_metadata_change_at=health_metadata_calls+2;
        assert(!sample()&&report.status==re4::HealthReportStatus::Unstable);health_metadata_change_at=0;
        build_supported=false;const auto before_health=health_ipc_reads;re4::LiveDiagnosticSnapshot unsupported;
        assert(!link.read_diagnostic_snapshot(unsupported)&&unsupported.status==re4::LiveDiagnosticStatus::UnsupportedBuild&&health_ipc_reads==before_health);build_supported=true;
        assert(write_count==0);link.close();
    }
    {
        reset();GameLink client;client.initialize();assert(!client.request_money()&&write_count==0);
        assert(client.connect());auto block=snapshot();block.money_availability=1;
        std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(client.refresh_money()&&client.money.availability==re4::MoneyAvailability::Ready&&write_count==0);
        assert(client.request_money()&&write_count==1&&write_sizes.back()==4&&snapshot().money_request==1);
        assert(!client.request_money()&&write_count==1);
        block=snapshot();block.money_ack=1;block.money_result=1;block.money_availability=2;
        std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(!client.request_money()&&client.money.availability==re4::MoneyAvailability::Claimed&&write_count==1);
        block.money_availability=1;block.money_result=99;std::memcpy(memory.data()+8,&block,sizeof(block));
        assert(!client.refresh_money()&&!client.request_money()&&write_count==1);
        block.money_result=0;std::memcpy(memory.data()+8,&block,sizeof(block));fail_write=2;
        assert(!client.request_money()&&snapshot().money_request==1);
        pid=99;assert(!client.request_money());client.close();
    }
    {
        reset();GameLink worker;worker.initialize();re4::LiveDiagnosticSnapshot live;
        auto block=snapshot();std::memcpy(block.module_version,"0.28.17",8);block.frames=104;
        std::memcpy(memory.data()+8,&block,sizeof(block));
        using F=re4::HealthCaptureField;
        auto wire=re4::initial_health_capture();wire.sequence=2;wire.valid=1;
        wire.words[unsigned(F::PublishedUnit)]=58;wire.words[unsigned(F::ComputedUnit)]=58;
        wire.words[unsigned(F::ControlFrameLo)]=100;
        const auto store=[&]{std::memcpy(memory.data()+hp_capture_address-address,&wire,sizeof(wire));hp_capture_reads=0;};
        store();assert(worker.read_diagnostic_snapshot(live)&&live.connected&&hp_capture_reads==3&&write_count==0);
        assert(live.health_last_capture_valid&&live.health_last_capture.words[unsigned(F::PublishedUnit)]==58);
        assert(live.health_last_capture.words[unsigned(F::ControlFrameLo)]==100&&live.shared.frames==104);
        const auto reference=live.health_reference;
        for(unsigned field=0;field<6;++field){
            wire=re4::initial_health_capture();wire.sequence=2;wire.valid=1;
            if(field==0)wire.marker[0]^=1;
            if(field==1)wire.version=2;
            if(field==2)wire.bytes=159;
            if(field==3)wire.valid=0;
            if(field==4)wire.valid=2;
            if(field==5)wire.sequence=3;
            store();assert(worker.read_diagnostic_snapshot(live)&&live.connected&&!live.health_last_capture_valid&&hp_capture_reads<=6);
            assert(live.health_reference.status==reference.status&&live.health_reference.predicted_unit==reference.predicted_unit);
        }
        wire=re4::initial_health_capture();wire.sequence=2;wire.valid=1;
        for(unsigned race:{1u,2u}){store();hp_capture_race=race;
            assert(worker.read_diagnostic_snapshot(live)&&live.connected&&!live.health_last_capture_valid&&hp_capture_reads==6);
            hp_capture_race=0;
        }
        store();hp_capture_race=3;
        assert(worker.read_diagnostic_snapshot(live)&&live.health_last_capture_valid&&hp_capture_reads==6);
        hp_capture_race=0;wire.sequence=2;
        for(u64 failed:{hp_capture_address,hp_capture_sequence}){store();fail_read_address=failed;
            assert(worker.read_diagnostic_snapshot(live)&&live.connected&&!live.health_last_capture_valid);fail_read_address=0;}
        wire={};store();assert(worker.read_diagnostic_snapshot(live)&&!live.health_last_capture_valid);
        wire=re4::initial_health_capture();wire.sequence=2;wire.valid=1;
        for(const char* version:{"","0.28.16","0.28.1","0.28.017","0.28.17x","0.28.17.0","0.29.17","1.28.17","0.28.4294967296","0.28.12345678901"}){
            block=snapshot();std::memset(block.module_version,0,16);std::memcpy(block.module_version,version,std::strlen(version));
            std::memcpy(memory.data()+8,&block,sizeof(block));store();
            assert(worker.read_diagnostic_snapshot(live)&&!live.health_last_capture_valid&&hp_capture_reads==0);
        }
        for(const char* version:{"0.28.17","0.28.18","0.28.100"}){
            block=snapshot();std::memset(block.module_version,0,16);std::memcpy(block.module_version,version,std::strlen(version));
            std::memcpy(memory.data()+8,&block,sizeof(block));store();
            assert(worker.read_diagnostic_snapshot(live)&&live.health_last_capture_valid&&hp_capture_reads==3);
        }
        hp_capture_reads=0;assert(worker.refresh_popup()&&hp_capture_reads==0&&write_count==0);
        worker.close();
    }
    {
        reset();GameLink worker;worker.initialize();re4::LiveDiagnosticSnapshot live;
        // Missing/invalid optional memory must never disconnect an ABI23 module.
        assert(worker.read_diagnostic_snapshot(live)&&live.connected&&!live.emitters_valid&&write_count==0);
        auto wire=re4::initial_emitter_diagnostics();wire.sequence=2;wire.records[1][0]=17;wire.records[1][19]=0x2DAF40;
        const auto store=[&]{std::memcpy(memory.data()+emitter_address-address,&wire,sizeof(wire));emitter_reads=0;};
        store();assert(worker.read_diagnostic_snapshot(live)&&live.emitters_valid&&live.emitters.records[1][0]==17
            &&live.emitters.records[1][19]==0x2DAF40&&emitter_reads==3&&write_count==0);
        for(unsigned field=0;field<5;++field){
            wire=re4::initial_emitter_diagnostics();wire.sequence=2;
            if(field==0)wire.marker[0]^=1;
            if(field==1)wire.version=2;
            if(field==2)wire.bytes=351;
            if(field==3)wire.channel_count=5;
            if(field==4)wire.sequence=3;
            store();assert(worker.read_diagnostic_snapshot(live)&&live.connected&&!live.emitters_valid&&emitter_reads<=3);
        }
        wire=re4::initial_emitter_diagnostics();wire.sequence=2;
        for(unsigned race:{1u,2u,3u}){store();emitter_race=race;
            assert(worker.read_diagnostic_snapshot(live)&&live.connected&&!live.emitters_valid&&emitter_reads==3);emitter_race=0;}
        for(u64 failed:{emitter_address,emitter_sequence}){store();fail_read_address=failed;
            assert(worker.read_diagnostic_snapshot(live)&&live.connected&&!live.emitters_valid);fail_read_address=0;}
        store();assert(worker.read_diagnostic_snapshot(live)&&live.emitters_valid&&write_count==0);
        emitter_reads=0;assert(worker.refresh_popup()&&emitter_reads==0&&write_count==0);worker.close();
    }
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
