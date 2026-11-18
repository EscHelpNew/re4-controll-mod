#include "../source/re5_controls/game_link.cpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <map>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 IPC line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
constexpr u64 base=0x100000;
std::array<u8,0x50000> memory{};
u64 pid=42,title=re5::profile::title_id,mapping_count=1;
unsigned attach_result=0,metadata_result=0,process_result=0,program_result=0,info_result=0,service_result=0;
unsigned writes=0,reads=0,fail_write=0,metadata_calls=0,attach_calls=0,process_calls=0;
u64 metadata_pid=0,metadata_title=0;
bool change_on_metadata=false,limit_sessions=false;
unsigned service_opens=0,service_closes=0,open_sessions=0,info_refs=0,info_exits=0;
bool legacy_metadata=false;
bool correct_build=true,torn=false,zero_build_id=false;unsigned bad_build_byte=0;
constexpr u64 probe_base=0x10000000;
u64 main_base_value=0,main_size_value=0;
unsigned note_calls=0,note_result=0,note_kind=0;bool change_on_note=false;
unsigned reads_after_identity_change=0;
unsigned proof_calls=0,proof_failure=0,change_on_proof=0,change_on_control=0,control_calls=0;
unsigned change_title_on_note=0,change_title_on_proof=0;
bool tear_proof=false,fail_scan=false;
unsigned note_failure_call=0,note_corrupt_byte=32,fail_control_read=0;
constexpr u8 old_id[32]={0xC5,0x17,0xEC,0xBB,0x79,0xDE,0x97,0x33,0x8E,0x98,0x14,0x7A,0x6B,0x5B,0x5F,0x2B};
constexpr u8 latest_id[32]={0x4E,0x46,0x40,0x1B,0xF2,0xFE,0x53,0xA4,0x4B,0x69,0xCC,0x07,0xFD,0x95,0x1F,0xE6};
// Independent raw wire fixture, without a production identity serializer.
void install_proof(bool legacy){
    std::array<u8,112> proof{};std::memcpy(proof.data(),"RE5CTL_IMAGE1",13);proof[16]=1;proof[20]=1;
    std::memcpy(proof.data()+24,legacy?old_id:latest_id,32);
    const u8 sdk[32]={0xB2,0x87,0x2A,0x7B,0xCC,0x58,0x99,0x69,0xFB,0x7A,0xFB,0x8E,0x37,0x24,0x14,0xE4,0xFA,0xC5,0xA7,0x50};
    std::memcpy(proof.data()+56,sdk,32);
    for(unsigned i=0;i<8;++i){proof[88+i]=u8(probe_base>>(8*i));proof[96+i]=u8(0x26ac000ULL>>(8*i));}
    proof[104]=0x23;proof[105]=0x0C; // 3107 guarded immutable words, little-endian.
    std::memcpy(memory.data()+8+384,proof.data(),proof.size());
}
// Independent 192-byte optional diagnostic ABI fixture; no production serializer.
void runtime_word(unsigned offset,u64 value,unsigned size=4){
    for(unsigned i=0;i<size;++i)memory[8+496+offset+i]=u8(value>>(8*i));
}
void install_runtime(u32 request=0,u32 applied=0,u32 active=0){
    std::fill_n(memory.data()+8+496,192,u8(0));
    std::memcpy(memory.data()+8+496,"RE5CTL_RUN1",11);
    runtime_word(16,1);runtime_word(20,192);runtime_word(24,request);runtime_word(28,applied);runtime_word(32,active);
    runtime_word(40,123,8);runtime_word(40+3*8,17,8);runtime_word(40+7*8,29,8);
    runtime_word(168,0x123456789abcdef0ULL,8);runtime_word(176,0xfedcba9876543210ULL,8);runtime_word(184,0x42,8);
}
unsigned runtime_reads=0,runtime_change=0,runtime_title_change=0,runtime_tear=0,runtime_failure=0,proof_mutation=0;
constexpr unsigned camera_offset=688,camera_bytes=5808;
unsigned camera_reads=0,camera_change=0,camera_title_change=0,camera_tear=0,camera_failure=0;
u64 mapping_size=memory.size();
void camera_word(unsigned offset,u64 value,unsigned size=4){
 for(unsigned i=0;i<size;++i)memory[8+camera_offset+offset+i]=u8(value>>(8*i));
}
void install_camera(u32 sequence=8){
 // Independent raw5808-byte ABI fixture; no native camera serializer.
 for(unsigned i=0;i<camera_bytes;++i)memory[8+camera_offset+i]=u8(i*37+11);
 std::fill_n(memory.data()+8+camera_offset,16,u8(0));
 std::memcpy(memory.data()+8+camera_offset,"RE5CTL_CAM1",11);
 camera_word(16,1);camera_word(20,camera_bytes);camera_word(24,sequence);camera_word(28,17);
 camera_word(32,UINT64_MAX,8);camera_word(40,UINT64_MAX,8);
}
std::vector<u8> decoded_camera(const std::string& text){
 const auto marker=text.find("\"camera_probe\":{");assert(marker!=std::string::npos);
 const auto start=text.find("\"data\":\"",marker);assert(start!=std::string::npos);
 const auto first=start+8,last=text.find('"',first);assert(last!=std::string::npos&&last-first==7744);
 std::vector<u8> out;unsigned bits=0,accumulator=0;
 for(auto at=first;at<last;++at){const char c=text[at];unsigned value=0;
  if(c>='A'&&c<='Z')value=unsigned(c-'A');else if(c>='a'&&c<='z')value=unsigned(c-'a'+26);
  else if(c>='0'&&c<='9')value=unsigned(c-'0'+52);else if(c=='+')value=62;else{assert(c=='/');value=63;}
  accumulator=(accumulator<<6)|value;bits+=6;if(bits>=8){bits-=8;out.push_back(u8(accumulator>>bits));}
 }
 assert(out.size()==camera_bytes&&bits==0);return out;
}
std::vector<u64> write_addresses;
std::map<u64,std::vector<u8>> route_fields;
unsigned route_reads=0,route_fail=0,route_change=0,route_bytes=0;
u64 route_fail_address=0,route_change_address=0,route_title_address=0;
template<class T>void route_put(u64 address,T value){std::vector<u8> bytes(sizeof(value));std::memcpy(bytes.data(),&value,sizeof(value));route_fields[address]=bytes;}
void seed_route(){
 constexpr u64 registry=0x30000000,actor=0x31000000,wrapper=0x32000000,pads=0x33000000;
 route_put(probe_base+0x1CF8CC8,registry);route_put(probe_base+0x26A9288,pads);
 for(unsigned i=0;i<4;++i)route_put(registry+0x30+i*8,i==0?actor:u64(0));
 route_put(actor+0x3F40,u32(0));route_put(actor+0x3F2C,u8(0));route_put(actor+0x3F49,u8(0));route_put(actor+0x3EE8,wrapper);
 route_put(wrapper+0x48,actor);route_put(wrapper+0x34C,u32(0));
 route_put(pads+0x8C98,u32(8));route_put(pads+0x8C94,u32(8));
 route_put(pads+8*0xEE0+0x670,u32(32));route_put(pads+8*0xEE0+0x674,u32(0x10));
}
void seed_actor_state(u64 actor=0x31000000,bool legacy=true){
    const u64 camera=actor+0x500000,action=actor+0x600000;
    route_put(actor+8,u16(0x201));route_put(actor+0x1B40,u32(0x1B));
    route_put(actor+0x48,actor+0x7700);route_put(actor+0x54,u16(3));route_put(actor+0x56,u16(3));
    route_put(actor+0x70,std::array<u32,4>{0x7FC00000,0x80000000,0x3F800000,0x3F000000});
    for(auto offset:{0x1B10,0x168E})route_put(actor+offset,u8(1));
    for(auto offset:{0x1A71,0x1B4C,0x3AD8,0x1AB0,0x2708,0x3F0E,0x29B6,0x29B8,0x2701,0x29C8})route_put(actor+offset,u8(0));
    route_put(actor+0x1EB8,std::int16_t(700));route_put(actor+0x1EBA,std::int16_t(1000));
    route_put(actor+0x80,std::array<float,3>{1,1,1});route_put(actor+0x1570,std::array<float,3>{1,1,1});
    route_put(actor+0x1B60,action);route_put(action+0x30,actor);route_put(actor+0x15D0,u64(0x40000000));
    route_put(actor+0x8C0,u32(136));route_put(actor+0x8C8,u64(0x41000000));route_put(actor+0x8D0,u64(0x42000000));
    std::array<u8,64> matrix;matrix.fill(255);route_put(actor+0x90,matrix);route_put(actor+0xD0,matrix);
    const u64 slot=0x50000000,owner=0x51000000,context=0x52000000,stage=0x53000000;
    route_put(probe_base+0x1CD9FC0,slot);route_put(slot,owner);route_put(owner+0x584E0,context);route_put(context+0x64,u32(0));
    route_put(probe_base+0x1CF7A40,stage);route_put(stage+0x2C,std::int32_t(3));route_put(stage+3*0x5F40+0x2AF00,u32(0x1234));
    route_put(probe_base+0x1AAEF28,probe_base+0x1CFC510);
    route_put(actor+0x1A80,camera);route_put(camera,probe_base+(legacy?re5::profile::legacy_offset(0x1AD2510):0x1AD2510));
    route_put(camera+0x130,actor);route_put(camera+0x2EC1,u8(1));route_put(camera+0x198,u64(1));route_put(camera+0x1A0,u64(2));route_put(camera+0xDA0,u64(3));
    std::array<u32,12> pose;pose.fill(0x7FC00000); // NaNs remain exact hex, valid JSON.
    route_put(camera+0x60,pose);route_put(camera+0x210,pose);route_put(camera+0x630,pose);
}
std::vector<std::size_t> sizes;
re5::SharedControl snapshot(){re5::SharedControl c{};std::memcpy(&c,memory.data()+8,sizeof(c));return c;}
void reset(){
    camera_reads=camera_change=camera_title_change=camera_tear=camera_failure=0;mapping_size=memory.size();
    route_fields.clear();route_reads=route_fail=route_change=route_bytes=0;route_fail_address=route_change_address=route_title_address=0;
    runtime_reads=runtime_change=runtime_title_change=runtime_tear=runtime_failure=proof_mutation=0;write_addresses.clear();
    memory.fill(0);pid=42;title=re5::profile::title_id;mapping_count=1;
    attach_result=metadata_result=process_result=program_result=info_result=service_result=0;
    writes=reads=fail_write=metadata_calls=attach_calls=process_calls=0;
    service_opens=service_closes=open_sessions=info_refs=info_exits=0;limit_sessions=false;
    metadata_pid=metadata_title=0;change_on_metadata=false;correct_build=true;torn=false;bad_build_byte=0;sizes.clear();
    reads_after_identity_change=0;legacy_metadata=false;proof_calls=proof_failure=change_on_proof=change_on_control=control_calls=0;
    change_title_on_note=change_title_on_proof=0;tear_proof=fail_scan=false;
    note_failure_call=fail_control_read=0;note_corrupt_byte=32;
    zero_build_id=false;main_base_value=main_size_value=0;note_calls=note_result=note_kind=0;change_on_note=false;
    auto c=re5::initial_shared_control();c.status=1;c.capabilities=re5::kKnownCapabilities;c.sequence=4;c.applied_sequence=4;
    c.sampler_overrides=7;
    std::memcpy(memory.data()+8,&c,sizeof(c));
}
}
Result pminfoInitialize(){if(!info_result)++info_refs;return info_result;}
void pminfoExit(){assert(info_refs);--info_refs;++info_exits;}
Result smGetService(Service* s,const char*){
    ++service_opens;
    if(service_result)return service_result;
    if(limit_sessions&&open_sessions)return 0xD001;
    s->active=true;++open_sessions;return 0;
}
bool serviceIsActive(Service* s){return s->active;}
void serviceClose(Service* s){assert(s->active&&open_sessions);s->active=false;--open_sessions;++service_closes;}
Result pmdmntGetApplicationProcessId(u64* out){++process_calls;*out=pid;return process_result;}
Result pminfoGetProgramId(u64* out,u64){*out=title;return program_result;}
Result test_dispatch(Service*,unsigned cmd,const void* in,std::size_t,void* out,std::size_t,TestDispatchParams args){
    if(limit_sessions)assert(open_sessions==1);
    if(cmd==65003){++attach_calls;return attach_result;}
    if(cmd==65002){++metadata_calls;if(metadata_result)return metadata_result;auto* m=static_cast<Re5Metadata*>(out);*m={};
        m->pid=metadata_pid?metadata_pid:pid;m->title=metadata_title?metadata_title:title;
        if(change_on_metadata){++pid;title=1;}
        m->main={main_base_value,main_size_value};
        if(!zero_build_id)std::memcpy(m->build_id,legacy_metadata?old_id:latest_id,32);
        if(!correct_build)m->build_id[bad_build_byte]^=1;
        return 0;}
    if(cmd==65101){*static_cast<u64*>(out)=*static_cast<const u64*>(in)?0:mapping_count;
        auto* maps=static_cast<MemoryInfo*>(const_cast<void*>(args.buffers[0].data));maps[0]={base,mapping_size,MemType_CodeMutable,Perm_Rw};return 0;}
    assert(cmd==65102||cmd==65103);
    struct Range{u64 address,size;};const auto r=*static_cast<const Range*>(in);
    if(cmd==65102&&(pid!=42||title!=re5::profile::title_id))++reads_after_identity_change;
    if(cmd==65102&&(r.address==main_base_value+0x1a988a4||r.address==main_base_value+0x1a988d4)){
        ++note_calls;++reads;assert(r.size==32);if(note_result)return note_result;
        if(note_failure_call==note_calls)return 0xCAFE;
        std::array<u8,32> note{};const u8 header[]={4,0,0,0,16,0,0,0,3,0,0,0,'G','N','U',0};
        std::memcpy(note.data(),header,sizeof(header));
        const bool latest=r.address==main_base_value+0x1a988a4;
        if((note_kind==0||note_kind==2||note_kind==4||note_kind==5)&&latest)std::memcpy(note.data()+16,re5::profile::build_id.data(),16);
        else if((note_kind==1||note_kind==4)&&!latest){const u8 old[]={0xC5,0x17,0xEC,0xBB,0x79,0xDE,0x97,0x33,0x8E,0x98,0x14,0x7A,0x6B,0x5B,0x5F,0x2B};std::memcpy(note.data()+16,old,16);}
        if(note_kind==2)note[0]=5;
        if(note_kind==5&&!latest)note[16]=0x12;
        if(note_kind==6&&latest)std::memcpy(note.data()+16,old_id,16);
        if(latest&&note_corrupt_byte<32)note[note_corrupt_byte]^=1;
        std::memcpy(const_cast<void*>(args.buffers[0].data),note.data(),note.size());
        if(change_on_note){++pid;title=1;}
        if(change_title_on_note==note_calls)title=1;
        return 0;
    }
    if(cmd==65102&&(r.address<base||r.address-base+r.size>memory.size())){
        ++route_reads;route_bytes+=unsigned(r.size);assert(r.size<=128);
        if(route_reads==route_change||r.address==route_change_address)++pid;
        if(r.address==route_title_address)title=1;
        if(route_reads==route_fail||r.address==route_fail_address)return 0xBEEF;
        auto found=route_fields.find(r.address);if(found==route_fields.end()||found->second.size()!=r.size)return 0xBADD;
        std::memcpy(const_cast<void*>(args.buffers[0].data),found->second.data(),r.size);return 0;
    }
    assert(r.address>=base&&r.address-base+r.size<=memory.size());auto* p=memory.data()+r.address-base;
    if(cmd==65102){++reads;
        if(r.address==base+8+camera_offset){
            ++camera_reads;if(camera_failure==camera_reads)return 0xCAFE;
            if(camera_change==camera_reads)++pid;
            if(camera_title_change==camera_reads)title=1;
            if(camera_tear==camera_reads)camera_word(24,10);
        }
        if(r.address==base+8+496){
            ++runtime_reads;
            if(runtime_failure==runtime_reads)return 0xCAFE;
            if(runtime_change==runtime_reads)++pid;
            if(runtime_title_change==runtime_reads)title=1;
            if(runtime_tear==runtime_reads)runtime_word(28,1);
        }
        if(r.address==base+8+384&&r.size==112){
            ++proof_calls;if(proof_mutation==proof_calls)memory[8+384+56]^=1;
            if(proof_failure==proof_calls)return 0xCAFE;
            if(change_on_proof==proof_calls)++pid;
            if(change_title_on_proof==proof_calls)title=1;
            if(tear_proof&&proof_calls==2)memory[8+384+108]=1;
        }
        if(r.address==base+8&&r.size==sizeof(re5::SharedControl)){
            ++control_calls;if(change_on_control==control_calls)++pid;
            if(fail_control_read==control_calls)return 0xCAFE;
        }
        if(fail_scan&&r.size>sizeof(re5::SharedControl))return 0xCAFE;
        if(torn&&r.size==sizeof(re5::SharedControl)){auto c=snapshot();c.sequence+=2;c.applied_sequence=c.sequence;std::memcpy(memory.data()+8,&c,sizeof(c));}std::memcpy(const_cast<void*>(args.buffers[0].data),p,r.size);}
    else{++writes;sizes.push_back(r.size);write_addresses.push_back(r.address);if(writes==fail_write)return 5;std::memcpy(p,args.buffers[0].data,r.size);}
    return 0;
}
int main(){
    re5::profile::selected_build=re5::profile::Build::None; // Overlay recovery never selects native address globals.
    // Rejecting all zero IDs prevents valid old/latest native modules from being configured.
    Re5GameLink probe_link;
    char probe_json[12288];
    auto inspect_route=[&](){auto count=probe_link.diagnostic_snapshot(probe_json,sizeof(probe_json));assert(count>0&&count<sizeof(probe_json));return std::string(probe_json,count);};
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=1;install_proof(true);seed_route();
    probe_link.initialize();auto route_text=inspect_route();
    assert(route_text.find("\"input_routes\":{")!=std::string::npos);
    assert(route_text.find("\"actor_state\":{")!=std::string::npos);
    assert(route_text.find("\"resolved\":true")!=std::string::npos);
    assert(route_text.find("\"stick_id\":32,\"stick_style\":2")!=std::string::npos);
    assert(route_text.find("\"read_count\":16,\"last_reads\":[\"000000003200034C/4/0000000000000000/00000000\"")!=std::string::npos);
    assert(route_text.find("0000000033007D74/4/0000000000000010/00000000")!=std::string::npos);
    assert(route_text.find("0000000031003EE8/8/")==std::string::npos); // Evicted from the last-eight ring.
    assert(route_reads>10&&route_reads<=4*24+5+112&&writes==0&&service_opens==1);probe_link.close();
    for(unsigned failure:{1u,5u,10u}){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=1;install_proof(true);seed_route();route_fail=failure;
        probe_link.initialize();route_text=inspect_route();assert(route_text.find("\"resolved\":true")==std::string::npos);
        if(failure!=1)assert(route_text.find("/0000BEEF")!=std::string::npos);
        assert(route_reads<=4*24+5+112&&writes==0);probe_link.close();
    }
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=1;install_proof(true);seed_route();
    route_put(u64(0x31000000)+0x3F40,u32(1));probe_link.initialize();route_text=inspect_route();assert(route_text.find("\"resolved\":true")==std::string::npos&&writes==0);probe_link.close();
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=1;install_proof(true);seed_route();route_change=5;
    probe_link.initialize();route_text=inspect_route();assert(route_text.find("\"input_routes\":null")!=std::string::npos);
    assert(route_reads==5&&writes==0);probe_link.close();
    reset();correct_build=false;main_base_value=probe_base;main_size_value=0x26ac000;seed_route();probe_link.initialize();route_text=inspect_route();
    assert(route_reads==0&&writes==0&&route_text.find("\"input_routes\":null")!=std::string::npos);probe_link.close();
    reset();seed_route();Re5InputProbe bounded{nullptr,42};u64 value=0;
    for(unsigned i=0;i<24;++i)assert(Re5InputProbe::read(&bounded,probe_base+0x1CF8CC8,&value,8));
    assert(!Re5InputProbe::read(&bounded,probe_base+0x1CF8CC8,&value,8)&&route_reads==24);
    Re5InputProbe overflowing{nullptr,42};
    assert(!Re5InputProbe::read(&overflowing,UINT64_MAX,&value,8)&&overflowing.count==0&&route_reads==24);
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=1;install_proof(true);seed_route();
    for(unsigned i=1;i<4;++i){
        const u64 actor=0x31000000+i*0x10000,wrapper=0x32000000+i*0x10000;
        route_put(u64(0x30000000)+0x30+i*8,actor);route_put(actor+0x3F40,u32(0));route_put(actor+0x3F2C,u8(i));
        route_put(actor+0x3F49,u8(0));route_put(actor+0x3EE8,wrapper);route_put(wrapper+0x48,actor);route_put(wrapper+0x34C,u32(0));
    }
    probe_link.initialize();route_text=inspect_route();
    for(unsigned i=0;i<4;++i)assert(route_text.find("\"slot\":"+std::to_string(i))!=std::string::npos);
    assert(route_reads<=4*24+5+112&&route_text.size()<12288&&writes==0);
    char probe_tiny[2]={'X','X'};assert(!probe_link.diagnostic_snapshot(probe_tiny,sizeof(probe_tiny))&&probe_tiny[0]==0&&writes==0);probe_link.close();
    // Authenticated state uses actual gate widths, safe raw float encoding, and camera ownership.
    auto actor_setup=[&](bool legacy=true){reset();legacy_metadata=legacy;main_base_value=probe_base;main_size_value=0x26ac000;
        install_proof(legacy);seed_route();seed_actor_state(0x31000000,legacy);};
    for(bool legacy:{false,true}){
        actor_setup(legacy);probe_link.initialize();route_text=inspect_route();
        assert(route_text.find("\"type\":513,\"action\":27,\"evaluator\":1")!=std::string::npos);
        assert(route_text.find("\"hp\":700,\"max_hp\":1000")!=std::string::npos);
        assert(route_text.find("\"parent\":\"0000000031007700\",\"rotation_order\":3,\"world_mode\":3")!=std::string::npos);
        assert(route_text.find("\"quaternion_hex\":\"0000C07F000000800000803F0000003F\"")!=std::string::npos);
        assert(route_text.find("\"room\":4660")!=std::string::npos);
        assert(route_text.find("\"camera_state\":{\"desired_block\":1")!=std::string::npos);
        assert(route_text.find("\"persistent_hex\":\"0000C07F")!=std::string::npos);
        assert(route_text.find("\"actor_state_failures\":0,\"actor_state_limited\":false")!=std::string::npos);
        assert(writes==0&&reads_after_identity_change==0);probe_link.close();
    }
    actor_setup();route_fail_address=0x31000000+0x1B10;probe_link.initialize();route_text=inspect_route();
    assert(route_text.find("\"evaluator\":null")!=std::string::npos&&route_text.find("\"ik\":1")!=std::string::npos);
    assert(route_text.find("\"actor_state_failures\":1")!=std::string::npos&&writes==0);probe_link.close();
    for(bool change_title:{false,true}){
        actor_setup();if(change_title)route_title_address=0x31000000+0x1B10;else route_change_address=0x31000000+0x1B10;
        probe_link.initialize();route_text=inspect_route();assert(route_text.find("\"input_routes\":null")!=std::string::npos);
        assert(reads_after_identity_change==0&&writes==0);probe_link.close();
    }
    actor_setup();route_put(u64(0x31500000)+0x130,u64(0xDEAD));probe_link.initialize();route_text=inspect_route();
    assert(route_text.find("\"camera_state\":null")!=std::string::npos&&writes==0);probe_link.close();
    actor_setup();memory[8+384+56]^=1;probe_link.initialize();route_text=inspect_route();
    assert(route_text.find("\"actor_state\":null")!=std::string::npos&&route_text.find("\"type\":513")==std::string::npos&&writes==0);probe_link.close();
    // Shared byte/read caps count attempted service bytes, and stop without another read.
    reset();std::array<u8,128> block{};route_put(u64(0x60000000),block);
    Re5ActorProbe byte_bound{nullptr,42};
    for(unsigned i=0;i<32;++i)assert(byte_bound.read(0x60000000,0,block.data(),128));
    assert(byte_bound.bytes==4096&&byte_bound.count==32&&route_bytes==4096);
    assert(!byte_bound.read(0x60000000,0,block.data(),1)&&byte_bound.limited&&route_reads==32);
    reset();route_put(u64(0x60000000),u8(7));Re5ActorProbe read_bound{nullptr,42};u8 byte=0;
    for(unsigned i=0;i<112;++i)assert(read_bound.get(0x60000000,0,byte));
    assert(!read_bound.get(0x60000000,0,byte)&&read_bound.limited&&route_reads==112&&read_bound.bytes==112);
    Re5ActorProbe address_bound{nullptr,42};
    assert(!address_bound.read(UINT64_MAX-3,4,&byte,1)&&address_bound.count==0&&route_reads==112);
    // Four retained routes, two actor states and maximal native counters fit the transport.
    actor_setup();install_runtime(1,1,1);
    for(unsigned i=0;i<16;++i)runtime_word(40+i*8,UINT64_MAX,8);
    for(unsigned i=1;i<4;++i){
        const u64 actor=0x31000000+i*0x10000,wrapper=0x32000000+i*0x10000;
        route_put(u64(0x30000000)+0x30+i*8,actor);route_put(actor+0x3F40,u32(0));route_put(actor+0x3F2C,u8(i));
        route_put(actor+0x3F49,u8(i%2));route_put(actor+0x3EE8,wrapper);route_put(wrapper+0x48,actor);route_put(wrapper+0x34C,u32(0));
        seed_actor_state(actor);
    }
    route_put(u64(0x33000000)+0xEE0+0x670,u32(1));route_put(u64(0x33000000)+0xEE0+0x674,u32(1));
    probe_link.initialize();route_text=inspect_route();
    assert(route_text.find("\"slot\":3")!=std::string::npos&&route_text.find("\"actor_state_limited\":false")!=std::string::npos);
    std::size_t states=0,position=0;while((position=route_text.find("\"actor_state\":{",position))!=std::string::npos){++states;++position;}
    assert(states==2&&route_text.size()<12288&&route_reads<=4*24+5+112&&route_bytes<=4*24*8+4*8+8+4096&&writes==0);
    const auto actor_length=route_text.size();std::vector<char> actor_exact(actor_length+2,'!');
    assert(!probe_link.diagnostic_snapshot(actor_exact.data(),actor_length)&&actor_exact[0]==0&&actor_exact[actor_length]=='!');
    assert(probe_link.diagnostic_snapshot(actor_exact.data(),actor_length+1)==actor_length&&actor_exact[actor_length]==0&&actor_exact[actor_length+1]=='!'&&writes==0);
    if(std::getenv("RE5_TEST_ACTOR_JSON"))std::puts(actor_exact.data());
    char route_tiny[5]={'!','!','!','!','!'};
    re5_input_probe(nullptr,probe_base,42,route_tiny,sizeof(route_tiny),Re5Build::Legacy,base+8);
    assert(!std::strcmp(route_tiny,"null")&&writes==0);probe_link.close();
    // Only explicit diagnostics may request the optional native two-second capture.
    Re5GameLink runtime_link;
    auto runtime_setup=[&](){reset();legacy_metadata=true;main_base_value=probe_base;main_size_value=0x26ac000;install_proof(true);};
    auto runtime_snapshot=[&](){const auto n=runtime_link.diagnostic_snapshot(probe_json,sizeof(probe_json));
        assert(n>0&&n<sizeof(probe_json)&&probe_json[n-1]=='}');return std::string(probe_json,n);};
    runtime_setup();runtime_link.initialize();auto runtime_text=runtime_snapshot();
    assert(runtime_text.find("\"camera_probe\":null")!=std::string::npos);
    assert(runtime_text.find("\"camera_probe_status\":\"unavailable\"")!=std::string::npos);
    assert(runtime_text.find("\"runtime\":null")!=std::string::npos&&writes==0);runtime_link.close();
    runtime_setup();install_runtime();runtime_link.initialize();assert(runtime_link.connect()&&writes==0&&runtime_reads==0);
    assert(runtime_link.apply(re5::Settings{})&&writes==3&&runtime_reads==0);
    auto runtime_control=snapshot();runtime_control.applied_sequence=runtime_control.sequence;
    std::memcpy(memory.data()+8,&runtime_control,sizeof(runtime_control));writes=0;sizes.clear();write_addresses.clear();
    const auto runtime_before=memory;runtime_text=runtime_snapshot();
    for(unsigned i=0;i<memory.size();++i)if(i<528||i>=532)assert(memory[i]==runtime_before[i]);
    assert(runtime_text.find("\"runtime\":{")!=std::string::npos);
    assert(runtime_text.find("\"request_sequence\":0,\"applied_sequence\":0,\"requested_sequence\":1")!=std::string::npos);
    assert(runtime_text.find("\"pad_calls\":123")!=std::string::npos);
    assert(runtime_text.find("\"input_rejected_thread\":17")!=std::string::npos);
    assert(runtime_text.find("\"fresh_rejected_thread\":29")!=std::string::npos);
    assert(runtime_text.find("\"input_thread\":\"123456789ABCDEF0\"")!=std::string::npos);
    assert(runtime_text.find("\"camera_thread\":\"FEDCBA9876543210\"")!=std::string::npos);
    assert(runtime_text.find("\"pinned_thread\":\"0000000000000042\"")!=std::string::npos);
    assert(writes==1&&(sizes==std::vector<std::size_t>{4})&&(write_addresses==std::vector<u64>{base+528}));
    assert(memory[528]==1&&proof_calls>=4&&service_opens==1);runtime_link.close();
    for(unsigned state=0;state<2;++state){
        runtime_setup();install_runtime(7,state?6:7,state?0:1);runtime_link.initialize();runtime_text=runtime_snapshot();
        assert(runtime_text.find("\"requested_sequence\":null")!=std::string::npos);
        assert(runtime_text.find("\"pad_calls\":123")!=std::string::npos&&writes==0);runtime_link.close();
    }
    runtime_setup();install_runtime(7,7,0);runtime_link.initialize();runtime_text=runtime_snapshot();
    assert(runtime_text.find("\"request_sequence\":7,\"applied_sequence\":7,\"requested_sequence\":8")!=std::string::npos);
    assert(runtime_text.find("\"pad_calls\":123")!=std::string::npos&&memory[528]==8&&writes==1);runtime_link.close();
    runtime_setup();install_runtime(0xffffffff,0xffffffff,0);runtime_link.initialize();runtime_text=runtime_snapshot();
    assert(runtime_text.find("\"requested_sequence\":1")!=std::string::npos&&writes==1&&memory[528]==1);runtime_link.close();
    // Bad optional headers, native proof, or identity changes must never authorize a write.
    for(unsigned bad=0;bad<17;++bad){
        runtime_setup();install_runtime();
        if(bad==0)memory[8+496]^=1;
        if(bad==1)memory[8+496+15]=1;
        if(bad==2)runtime_word(16,2);
        if(bad==3)runtime_word(20,191);
        if(bad==4)runtime_word(32,2);
        if(bad==5)runtime_word(36,1);
        if(bad==6)memory[8+384+24]^=1;
        if(bad==7)correct_build=false;
        if(bad==8)runtime_change=1;
        if(bad==9)runtime_title_change=2;
        if(bad==10)runtime_tear=2;
        if(bad==11)change_on_proof=3;
        if(bad==12)change_title_on_proof=4;
        if(bad==13)proof_mutation=3;
        if(bad==14)proof_failure=4;
        if(bad==15)runtime_tear=3;
        if(bad==16)runtime_tear=4;
        runtime_link.initialize();runtime_text=runtime_snapshot();
        assert(runtime_text.find("\"runtime\":null")!=std::string::npos&&writes==0&&reads_after_identity_change==0);
        runtime_link.close();
    }
    for(unsigned failure=1;failure<=4;++failure){
        runtime_setup();install_runtime();runtime_failure=failure;runtime_link.initialize();runtime_text=runtime_snapshot();
        assert(runtime_text.find("\"runtime\":null")!=std::string::npos&&writes==0);runtime_link.close();
    }
    runtime_setup();install_runtime();fail_write=1;runtime_link.initialize();runtime_text=runtime_snapshot();
    assert(runtime_text.find("\"requested_sequence\":null")!=std::string::npos&&writes==1&&memory[528]==0);
    fail_write=0;runtime_text=runtime_snapshot();assert(writes==2&&memory[528]==1);runtime_link.close();
    runtime_setup();install_runtime();runtime_link.initialize();const auto overflow_reads=reads;
    const auto overflow=re5_runtime_capture(nullptr,UINT64_MAX-100,Re5Build::Legacy,{probe_base,0x26ac000},42);
    assert(!overflow.address&&reads==overflow_reads&&writes==0);runtime_link.close();
    // Zero metadata BuildIDs still require the existing GNU note and full native proof.
    runtime_setup();zero_build_id=true;note_kind=1;install_runtime();runtime_link.initialize();runtime_text=runtime_snapshot();
    assert(runtime_text.find("\"identity_source\":\"native_verified\"")!=std::string::npos&&writes==1&&proof_calls==8);runtime_link.close();
    // A capacity failure cannot consume a capture, and four route entries still fit with maximal counters.
    runtime_setup();install_runtime();runtime_link.initialize();char runtime_tiny[2]={'X','X'};
    assert(!runtime_link.diagnostic_snapshot(runtime_tiny,sizeof(runtime_tiny))&&runtime_tiny[0]==0&&writes==0);runtime_link.close();
    runtime_setup();install_runtime(1,1,1);seed_route();
    for(unsigned i=0;i<16;++i)runtime_word(40+i*8,UINT64_MAX,8);
    for(unsigned i=1;i<4;++i){
        const u64 actor=0x31000000+i*0x10000,wrapper=0x32000000+i*0x10000;
        route_put(u64(0x30000000)+0x30+i*8,actor);route_put(actor+0x3F40,u32(0));route_put(actor+0x3F2C,u8(i));
        route_put(actor+0x3F49,u8(0));route_put(actor+0x3EE8,wrapper);route_put(wrapper+0x48,actor);route_put(wrapper+0x34C,u32(0));
    }
    runtime_link.initialize();runtime_text=runtime_snapshot();
    assert(runtime_text.find("\"slot\":3")!=std::string::npos&&runtime_text.find("\"movement_rejected\":18446744073709551615")!=std::string::npos);
    assert(runtime_text.size()<12288&&writes==0&&service_opens==1);
    const auto runtime_length=runtime_text.size();std::vector<char> runtime_exact(runtime_length+2,'!');
    assert(!runtime_link.diagnostic_snapshot(runtime_exact.data(),runtime_length)&&runtime_exact[0]==0&&runtime_exact[runtime_length]=='!');
    assert(runtime_link.diagnostic_snapshot(runtime_exact.data(),runtime_length+1)==runtime_length
        &&runtime_exact[runtime_length]==0&&runtime_exact[runtime_length+1]=='!'&&writes==0);
    if(std::getenv("RE5_TEST_RUNTIME_JSON"))std::puts(runtime_exact.data());
    runtime_link.close();
    // Camera observations are optional, read-only and lossless even for NaNs.
    Re5GameLink camera_link;char camera_json[16384];
    auto camera_setup=[&](){actor_setup();install_runtime(17,17,1);install_camera();};
    auto camera_snapshot=[&](){const auto n=camera_link.diagnostic_snapshot(camera_json,sizeof(camera_json));
        assert(n>0&&n<sizeof(camera_json));return std::string(camera_json,n);};
    camera_setup();camera_link.initialize();assert(camera_link.connect()&&camera_reads==0&&writes==0);
    auto camera_text=camera_snapshot();assert(camera_text.find(R"("camera_probe_status":"ok")")!=std::string::npos);
    assert(camera_text.find(R"("encoding":"base64","abi":1,"bytes":5808)")!=std::string::npos);
    auto camera_decoded=decoded_camera(camera_text);assert(!std::memcmp(camera_decoded.data(),memory.data()+8+camera_offset,camera_bytes));
    assert(camera_reads==4&&writes==0);camera_link.close();
    for(unsigned bad=0;bad<6;++bad){camera_setup();
        if(bad==0)memory[8+camera_offset]^=1;
        if(bad==1)camera_word(16,2);
        if(bad==2)camera_word(20,0);
        if(bad==3)camera_word(20,5807);
        if(bad==4)camera_word(20,8193);
        if(bad==5)camera_word(24,9);
        camera_link.initialize();camera_text=camera_snapshot();assert(camera_text.find(R"("camera_probe":null)")!=std::string::npos);
        assert(camera_text.find(R"("status":"connected")")!=std::string::npos&&writes==0);camera_link.close();
    }
    for(unsigned changed:{2u,3u,4u}){camera_setup();camera_tear=changed;camera_link.initialize();camera_text=camera_snapshot();
        assert(camera_text.find(R"("camera_probe":null)")!=std::string::npos&&camera_text.find(R"("camera_probe_status":"changed")")!=std::string::npos);
        assert(writes==0);camera_link.close();
    }
    for(unsigned failed:{1u,2u,3u,4u}){camera_setup();camera_failure=failed;camera_link.initialize();camera_text=camera_snapshot();
        assert(camera_text.find(R"("camera_probe":null)")!=std::string::npos&&camera_text.find(R"("camera_probe_status":"read_failed")")!=std::string::npos);
        assert(writes==0);camera_link.close();
    }
    for(bool program:{false,true})for(unsigned changed:{1u,2u,3u,4u}){camera_setup();
        if(program)camera_title_change=changed;else camera_change=changed;
        camera_link.initialize();camera_text=camera_snapshot();assert(camera_text.find(R"("camera_probe":null)")!=std::string::npos);
        assert(camera_text.find(R"("camera_probe_status":"identity_changed")")!=std::string::npos&&writes==0&&reads_after_identity_change==0);camera_link.close();
    }
    // The full image proof also brackets the observation, including the late pair.
    for(unsigned mutation=0;mutation<4;++mutation){camera_setup();
        if(mutation==0)proof_mutation=7;
        if(mutation==1)proof_failure=8;
        if(mutation==2)change_on_proof=7;
        if(mutation==3)change_title_on_proof=8;
        camera_link.initialize();camera_text=camera_snapshot();
        assert(camera_text.find(R"("camera_probe":null)")!=std::string::npos&&writes==0&&reads_after_identity_change==0);camera_link.close();
    }
    camera_setup();install_runtime(17,17,0);camera_link.initialize();char camera_tiny[2]={'!','!'};
    assert(!camera_link.diagnostic_snapshot(camera_tiny,sizeof(camera_tiny))&&camera_tiny[0]==0&&writes==0);camera_link.close();
    camera_setup();mapping_size=8+camera_offset+47;camera_link.initialize();camera_text=camera_snapshot();
    assert(camera_reads==0&&camera_text.find(R"("camera_probe":null)")!=std::string::npos&&writes==0);camera_link.close();
    camera_setup();mapping_size=8+camera_offset+5807;camera_link.initialize();camera_text=camera_snapshot();
    assert(camera_reads==1&&camera_text.find(R"("camera_probe":null)")!=std::string::npos&&writes==0);camera_link.close();
    static re5::CameraProbe bounded_camera{};const auto camera_before_reads=reads;
    assert(!re5_camera_capture(nullptr,UINT64_MAX-100,8192,Re5Build::Legacy,{probe_base,0x26ac000},42,bounded_camera).value);
    assert(!re5_camera_capture(nullptr,base+8,8192,Re5Build::Legacy,{UINT64_MAX-3,0x26ac000},42,bounded_camera).value);
    assert(reads==camera_before_reads&&writes==0);
    // Full wire data and maximal existing counters/two actor states must fit16KiB.
    camera_setup();zero_build_id=true;note_kind=1;for(unsigned i=0;i<16;++i)runtime_word(40+i*8,UINT64_MAX,8);
    auto maximum_control=snapshot();maximum_control.sequence=maximum_control.applied_sequence=UINT32_MAX-1;
    maximum_control.settings_source=maximum_control.graphics_filter_latched=maximum_control.sampler_calls=maximum_control.sampler_overrides=UINT32_MAX;
    maximum_control.frames=UINT64_MAX;re5::Settings maximum_settings;
    for(auto& p:maximum_settings.players){
        p.analog_enabled=p.unified_controls=p.gyro_enabled=p.move_while_aiming=p.move_while_knife=p.move_while_reloading=p.free_camera=p.first_person=false;
        p.camera_gain=p.camera_return_speed=1.234567e-30f;p.third_person_distance=1.234567f;
    }
    assert(re5::encode_settings(maximum_settings,maximum_control.settings,sizeof(maximum_control.settings)));
    std::memcpy(memory.data()+8,&maximum_control,sizeof(maximum_control));
    for(unsigned i=1;i<4;++i){const u64 actor=0x31000000+i*0x10000,wrapper=0x32000000+i*0x10000;
        route_put(u64(0x30000000)+0x30+i*8,actor);route_put(actor+0x3F40,u32(0));route_put(actor+0x3F2C,u8(i));
        route_put(actor+0x3F49,u8(i%2));route_put(actor+0x3EE8,wrapper);route_put(wrapper+0x48,actor);route_put(wrapper+0x34C,u32(0));seed_actor_state(actor);
    }
    route_put(u64(0x33000000)+0xEE0+0x670,u32(1));route_put(u64(0x33000000)+0xEE0+0x674,u32(1));
    for(unsigned i=0;i<2;++i){const u64 actor=0x31000000+i*0x10000;
        route_put(actor+8,u16(UINT16_MAX));route_put(actor+0x1B40,u32(UINT32_MAX));
        for(auto offset:{0x1B10,0x1A71,0x1B4C,0x3AD8,0x1AB0,0x168E,0x2708,0x3F0E,0x29B6,0x2701,0x29C8})route_put(actor+offset,u8(255));
        route_put(actor+0x29B8,std::int8_t(-128));route_put(actor+0x1EB8,std::int16_t(-32768));route_put(actor+0x1EBA,std::int16_t(32767));
        route_put(actor+0x54,u16(UINT16_MAX));route_put(actor+0x56,u16(UINT16_MAX));route_put(actor+0x8C0,u32(UINT32_MAX));
        route_put(actor+0x500000+0x2EC1,u8(255));
    }
    route_put(u64(0x52000000)+0x64,u32(UINT32_MAX));route_put(u64(0x53000000)+0x2C,std::int32_t(63));
    route_put(u64(0x53000000)+63*0x5F40+0x2AF00,u32(UINT32_MAX));
    camera_link.initialize();camera_text=camera_snapshot();camera_decoded=decoded_camera(camera_text);
    assert(camera_text.find(R"("slot":3)")!=std::string::npos&&camera_text.find(R"("actor_state_limited":false)")!=std::string::npos);
    assert(camera_text.size()<16384&&writes==0);const auto camera_length=camera_text.size();
    std::vector<char> camera_exact(camera_length+2,'!');
    assert(!camera_link.diagnostic_snapshot(camera_exact.data(),camera_length)&&camera_exact[0]==0&&camera_exact[camera_length]=='!');
    assert(camera_link.diagnostic_snapshot(camera_exact.data(),camera_length+1)==camera_length&&camera_exact[camera_length]==0&&camera_exact[camera_length+1]=='!'&&writes==0);
    if(std::getenv("RE5_TEST_CAMERA_JSON")){std::puts(camera_exact.data());}
    camera_link.close();
    Re5GameLink recovered;
    for(bool legacy:{false,true}){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=legacy?1:0;
        install_proof(legacy);recovered.initialize();assert(recovered.connect());assert(note_calls==2&&proof_calls==2);
        re5::Settings edited;edited.players[1].gyro_speed=2.5f;
        assert(recovered.apply(edited)&&writes==3&&snapshot().sequence==6);
        assert(note_calls==4&&proof_calls==4);recovered.close();
    }
    reset();legacy_metadata=true;recovered.initialize();assert(recovered.connect());
    assert(recovered.apply(re5::Settings{})&&writes==3&&note_calls==0&&proof_calls==0);recovered.close();
    // Every immutable field and padded tail must be exact.
    for(unsigned byte:{0u,15u,16u,19u,20u,23u,24u,39u,40u,55u,56u,87u,88u,95u,96u,103u,104u,107u,108u,111u}){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=1;install_proof(true);
        memory[8+384+byte]^=1;recovered.initialize();assert(!recovered.connect());
        assert(!recovered.apply(re5::Settings{})&&writes==0);recovered.close();
    }
    for(unsigned failure=1;failure<=2;++failure){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;install_proof(false);
        proof_failure=failure;recovered.initialize();assert(!recovered.connect()&&writes==0);recovered.close();
    }
    for(unsigned mutation=0;mutation<8;++mutation){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;install_proof(false);
        if(mutation==0)change_on_proof=1;
        if(mutation==1)change_on_proof=2;
        if(mutation==2)change_title_on_proof=1;
        if(mutation==3)change_title_on_proof=2;
        if(mutation==4)tear_proof=true;
        if(mutation==5)change_on_control=1;
        if(mutation==6)change_title_on_note=1;
        if(mutation==7)change_title_on_note=2;
        recovered.initialize();assert(!recovered.connect()&&writes==0);
        assert(reads_after_identity_change==0);recovered.close();
    }
    for(unsigned mutation=0;mutation<12;++mutation){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=1;install_proof(true);
        recovered.initialize();assert(recovered.connect());
        if(mutation==0)memory[8+384+24]^=1;
        if(mutation==1)memory[8+384+56]^=1;
        if(mutation==2)change_on_proof=3;
        if(mutation==3)change_title_on_proof=4;
        if(mutation==4)change_on_control=control_calls+2;
        if(mutation==5)note_kind=0;
        if(mutation==6){zero_build_id=false;correct_build=false;bad_build_byte=31;}
        if(mutation==7){zero_build_id=false;legacy_metadata=false;}
        if(mutation==8)proof_failure=3;
        if(mutation==9)++main_base_value;
        if(mutation==10)metadata_title=1;
        if(mutation==11)change_on_note=true;
        assert(!recovered.apply(re5::Settings{})&&writes==0);recovered.close();
    }
    for(unsigned kind:{3u,4u,5u,6u}){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=kind;install_proof(false);
        recovered.initialize();assert(!recovered.connect());assert(!recovered.apply(re5::Settings{})&&writes==0);recovered.close();
    }
    reset();main_base_value=probe_base;main_size_value=0x26ac000;recovered.initialize();assert(recovered.connect());
    zero_build_id=true;assert(!recovered.apply(re5::Settings{})&&writes==0);recovered.close();

    // Each GNU header/descriptor byte is checked, and either failed probe rejects recovery.
    for(unsigned byte=0;byte<32;++byte){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;install_proof(false);
        note_corrupt_byte=byte;recovered.initialize();assert(!recovered.connect());
        assert(!recovered.apply(re5::Settings{})&&writes==0);recovered.close();
    }
    for(unsigned failed=1;failed<=2;++failed){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=1;install_proof(true);
        note_failure_call=failed;recovered.initialize();assert(!recovered.connect()&&writes==0);recovered.close();
    }
    for(unsigned failure=0;failure<4;++failure){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;install_proof(false);
        recovered.initialize();
        if(failure==0){fail_scan=true;assert(!recovered.connect());}
        if(failure==1){fail_control_read=1;assert(!recovered.connect());}
        if(failure>=2){assert(recovered.connect());fail_control_read=control_calls+failure-1;assert(!recovered.apply(re5::Settings{}));}
        assert(writes==0);recovered.close();
    }
    for(bool legacy:{false,true})for(unsigned byte:{0u,15u,16u,31u}){
        reset();legacy_metadata=legacy;correct_build=false;bad_build_byte=byte;
        main_base_value=probe_base;main_size_value=0x26ac000;note_kind=legacy?1:0;install_proof(legacy);
        recovered.initialize();assert(!recovered.connect());assert(!recovered.apply(re5::Settings{}));
        assert(reads==0&&note_calls==0&&proof_calls==0&&writes==0);recovered.close();
    }
    for(u64 size:{0ULL,0x26abfffULL,0x26ac001ULL,UINT64_MAX}){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=size;install_proof(false);
        recovered.initialize();assert(!recovered.connect()&&reads==0&&writes==0);recovered.close();
    }
    for(bool legacy:{false,true}){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x26ac000;note_kind=legacy?1:0;install_proof(legacy);
        recovered.initialize();char identity_json[4096];
        assert(recovered.diagnostic_snapshot(identity_json,sizeof(identity_json))>0);
        const std::string observed(identity_json);
        assert(observed.find("\"build_id\":\"0000000000000000000000000000000000000000000000000000000000000000\"")!=std::string::npos);
        assert(observed.find(legacy?"\"effective_build_id\":\"C517ECBB79DE97338E98147A6B5B5F2B00000000000000000000000000000000\""
            :"\"effective_build_id\":\"4E46401BF2FE53A44B69CC07FD951FE600000000000000000000000000000000\"")!=std::string::npos);
        assert(observed.find("\"identity_source\":\"native_verified\"")!=std::string::npos);
        assert(observed.find("\"supported\":true")!=std::string::npos&&note_calls==2&&proof_calls==4&&writes==0);
        assert(re5::profile::selected_build==re5::profile::Build::None);recovered.close();
    }
    // One dmnt:cht session is available: a diagnostic borrower must not open another.
    reset();limit_sessions=true;Re5GameLink owner,borrower;owner.initialize();assert(owner.connect());
    borrower.initialize_shared(owner);char shared_json[4096];
    assert(borrower.diagnostic_snapshot(shared_json,sizeof(shared_json))>0);
    assert(std::string(shared_json).find("\"status\":\"connected\"")!=std::string::npos);
    assert(service_opens==1&&open_sessions==1&&info_refs==2&&writes==0);
    borrower.close();assert(service_closes==0&&open_sessions==1&&info_refs==1);
    assert(owner.connect());borrower.close();assert(service_closes==0&&info_refs==1);
    owner.close();assert(service_closes==1&&open_sessions==0&&info_refs==0);

    // Reinitialization releases only this instance's old reference/owned session.
    reset();limit_sessions=true;owner.initialize();borrower.initialize_shared(owner);
    borrower.initialize_shared(owner);assert(info_refs==2&&info_exits==1&&service_opens==1);
    borrower.initialize();assert(service_opens==2&&open_sessions==1&&info_refs==2);
    assert(borrower.diagnostic_snapshot(shared_json,sizeof(shared_json))>0);
    assert(std::string(shared_json).find("\"service\":53249")!=std::string::npos);
    borrower.close();assert(open_sessions==1&&info_refs==1&&service_closes==0);
    borrower.initialize_shared(owner);assert(borrower.connect()&&service_opens==2&&info_refs==2);
    borrower.close();owner.close();assert(open_sessions==0&&info_refs==0&&service_closes==1);
    reset();limit_sessions=true;owner.initialize();owner.initialize();
    assert(open_sessions==1&&info_refs==1&&service_opens==2&&service_closes==1&&info_exits==1);
    owner.close();assert(open_sessions==0&&info_refs==0&&service_closes==2&&info_exits==2);
    reset();owner.initialize();borrower.initialize();assert(open_sessions==2&&info_refs==2);
    borrower.initialize_shared(owner);assert(open_sessions==1&&info_refs==2&&service_closes==1);
    borrower.close();assert(open_sessions==1&&info_refs==1&&service_closes==1);
    owner.close();assert(open_sessions==0&&info_refs==0&&service_closes==2);
    // A failed borrower pminfo ref or failed owner's service preserves the actual Result.
    reset();limit_sessions=true;owner.initialize();info_result=0xB001;borrower.initialize_shared(owner);
    assert(borrower.diagnostic_snapshot(shared_json,sizeof(shared_json))>0);
    assert(std::string(shared_json).find("\"pminfo\":45057")!=std::string::npos);
    borrower.close();assert(info_refs==1&&open_sessions==1&&service_closes==0);
    info_result=0;borrower.initialize_shared(owner);assert(borrower.connect()&&info_refs==2);
    borrower.close();owner.close();assert(info_refs==0&&open_sessions==0);
    reset();service_result=0xB002;owner.initialize();borrower.initialize_shared(owner);
    assert(borrower.diagnostic_snapshot(shared_json,sizeof(shared_json))>0);
    assert(std::string(shared_json).find("\"service\":45058")!=std::string::npos);
    assert(service_opens==1&&open_sessions==0&&service_closes==0&&info_refs==2);
    borrower.close();owner.close();assert(info_refs==0&&service_closes==0);
    // A metadata transport failure must never be labelled as an unknown build.
    reset();Re5GameLink diagnosed;diagnosed.initialize();metadata_result=0x1234;
    assert(!diagnosed.connect());assert(diagnosed.message=="Could not read RE5 metadata");
    assert(diagnosed.details.find("0x00001234")!=std::string::npos);
    assert(writes==0&&reads==0);diagnosed.close();
    // A real mismatch must expose all 32 bytes, including the padded tail.
    reset();correct_build=false;bad_build_byte=31;diagnosed.initialize();
    assert(!diagnosed.connect());
    assert(diagnosed.details.find("4E46401BF2FE53A44B69CC07FD951FE600000000000000000000000000000001")!=std::string::npos);
    assert(diagnosed.details.find("expected")!=std::string::npos);
    assert(writes==0&&reads==0);diagnosed.close();
    // Diagnosis refreshes every request, exposes actual metadata even on an unsupported build,
    // and never writes settings. Zero-ID note probes require a stable verified RE5 process.
    char json[4096];
    auto diagnose=[&](){const auto n=diagnosed.diagnostic_snapshot(json,sizeof(json));
        assert(n==std::strlen(json)&&n>0&&json[0]=='{'&&json[n-1]=='}');assert(writes==0);
        if(std::getenv("RE5_TEST_DIAGNOSTIC_JSON"))std::puts(json);
        return std::string(json);};
    // A known GNU note without native proof remains diagnostic evidence and cannot authorize apply.
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=re5::profile::main_size;diagnosed.initialize();
    auto missing=diagnose();
    assert(missing.find("\"status\":\"image_identity_unverified\"")!=std::string::npos);
    assert(missing.find("\"build_id\":\"0000000000000000000000000000000000000000000000000000000000000000\"")!=std::string::npos);
    assert(missing.find("\"main_extent\":{\"base\":\"0000000010000000\",\"size\":40550400}")!=std::string::npos);
    assert(missing.find("\"image_notes\":[")!=std::string::npos&&note_calls==2&&writes==0);
    assert(missing.find("\"valid\":true,\"build_id\":\"4E46401BF2FE53A44B69CC07FD951FE600000000000000000000000000000000\"")!=std::string::npos);
    assert(missing.find("\"supported\":false")!=std::string::npos&&missing.find("\"ipc\":null")!=std::string::npos);
    assert(missing.find("\"effective_build_id\":null")!=std::string::npos&&missing.find("\"identity_source\":\"unavailable\"")!=std::string::npos);
    assert(!diagnosed.apply(re5::Settings{})&&writes==0);diagnosed.close();
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=re5::profile::main_size;note_kind=1;diagnosed.initialize();
    missing=diagnose();assert(missing.find("C517ECBB79DE97338E98147A6B5B5F2B00000000000000000000000000000000")!=std::string::npos&&note_calls==2);diagnosed.close();
    for(unsigned bad=0;bad<5;++bad){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=re5::profile::main_size;
        if(bad==0)main_base_value=0;
        if(bad==1)++main_base_value;
        if(bad==2)main_size_value=0x1000;
        if(bad==3)main_base_value=UINT64_MAX-0xfff;
        if(bad==4)main_size_value=0x1a988b3;
        diagnosed.initialize();missing=diagnose();assert(!note_calls&&reads==0&&writes==0);
        assert(missing.find("\"read_result\":null")!=std::string::npos);diagnosed.close();
    }
    for(unsigned invalid=0;invalid<3;++invalid){
        reset();zero_build_id=true;main_base_value=probe_base;main_size_value=re5::profile::main_size;
        if(invalid==0)note_result=0xCAFE;else note_kind=invalid==1?2:3;
        diagnosed.initialize();missing=diagnose();assert(note_calls==2&&writes==0);
        assert(missing.find("\"valid\":true")==std::string::npos);
        if(invalid==0)assert(missing.find("\"read_result\":51966")!=std::string::npos);
        diagnosed.close();
    }
    // A PID/title change during a read discards even an already valid note.
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=re5::profile::main_size;change_on_note=true;
    diagnosed.initialize();missing=diagnose();assert(note_calls==1&&writes==0);
    assert(missing.find("\"status\":\"identity_changed\"")!=std::string::npos&&missing.find("\"image_notes\":null")!=std::string::npos);diagnosed.close();
    // A truncated main extent cannot recover identity even if it contains one whole note.
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=0x1a988c4;
    diagnosed.initialize();missing=diagnose();assert(note_calls==0&&reads==0&&writes==0);
    assert(missing.find("\"valid\":true")==std::string::npos);diagnosed.close();
    // A later valid metadata identity must not retain the diagnostic observations.
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=re5::profile::main_size;diagnosed.initialize();missing=diagnose();
    zero_build_id=false;note_calls=0;missing=diagnose();
    assert(!note_calls&&missing.find("\"image_notes\":null")!=std::string::npos&&missing.find("\"supported\":true")!=std::string::npos);diagnosed.close();
    reset();zero_build_id=true;main_base_value=probe_base;main_size_value=re5::profile::main_size;change_on_metadata=true;
    diagnosed.initialize();missing=diagnose();assert(!note_calls&&reads==0);diagnosed.close();
    reset();correct_build=false;bad_build_byte=31;diagnosed.initialize();
    auto text=diagnose();
    assert(text.find("\"schema\":1")!=std::string::npos&&text.find("\"game\":\"RE5\"")!=std::string::npos);
    assert(text.find("\"overlay_version\":\"")!=std::string::npos);
    assert(text.find("\"status\":\"unsupported_build\"")!=std::string::npos);
    assert(text.find("\"supported\":false")!=std::string::npos);
    assert(text.find("\"program_id\":\"010018100CD46000\"")!=std::string::npos);
    assert(text.find("\"metadata_program_id\":\"010018100CD46000\"")!=std::string::npos);
    assert(text.find("\"process_id\":\"000000000000002A\"")!=std::string::npos);
    assert(text.find("\"build_id\":\"4E46401BF2FE53A44B69CC07FD951FE600000000000000000000000000000001\"")!=std::string::npos);
    assert(text.find("\"expected_build_id\":\"4E46401BF2FE53A44B69CC07FD951FE600000000000000000000000000000000\"")!=std::string::npos);
    assert(reads==0&&text.find("\"ipc\":null")!=std::string::npos);
    // Changing games must discard previously observed metadata immediately.
    title=1;text=diagnose();assert(text.find("\"status\":\"no_game\"")!=std::string::npos);
    assert(text.find("\"build_id\":null")!=std::string::npos);
    assert(text.find("\"metadata_program_id\":null")!=std::string::npos);
    assert(metadata_calls==1&&reads==0);diagnosed.close();
    // Exact numeric transport errors distinguish inaccessible metadata from unsupported identity.
    reset();metadata_result=0x1234;diagnosed.initialize();text=diagnose();
    assert(text.find("\"status\":\"metadata_error\"")!=std::string::npos);
    assert(text.find("\"metadata\":4660")!=std::string::npos);
    assert(text.find("\"build_id\":null")!=std::string::npos&&reads==0);diagnosed.close();
    for(unsigned stage=0;stage<5;++stage){
        reset();
        if(stage==0)process_result=0xA101;
        if(stage==1)program_result=0xA102;
        if(stage==2)attach_result=0xA103;
        if(stage==3)info_result=0xA104;
        if(stage==4)service_result=0xA105;
        diagnosed.initialize();text=diagnose();
        constexpr const char* statuses[]={"process_error","program_error","attach_error","info_unavailable","service_unavailable"};
        assert(text.find(std::string("\"status\":\"")+statuses[stage]+"\"")!=std::string::npos);
        assert(text.find(std::to_string(0xA101+stage))!=std::string::npos);
        assert(reads==0&&metadata_calls==0);diagnosed.close();
    }
    for(unsigned wrong=0;wrong<3;++wrong){
        reset();if(wrong==0)metadata_pid=99;if(wrong==1)metadata_title=1;if(wrong==2)change_on_metadata=true;
        diagnosed.initialize();text=diagnose();assert(text.find("\"status\":\"identity_changed\"")!=std::string::npos);
        assert(text.find("\"supported\":false")!=std::string::npos&&reads==0);diagnosed.close();
    }
    reset();diagnosed.initialize();auto live=snapshot();live.frames=9876543210ULL;live.settings_source=2;
    std::memcpy(memory.data()+8,&live,sizeof(live));text=diagnose();
    assert(text.find("\"status\":\"connected\"")!=std::string::npos);
    assert(text.find("\"identity_source\":\"metadata\"")!=std::string::npos);
    assert(text.find("\"supported\":true")!=std::string::npos);
    assert(text.find("\"frames\":9876543210")!=std::string::npos);
    assert(text.find("\"sequence\":4")!=std::string::npos&&text.find("\"settings_source\":2")!=std::string::npos);
    assert(text.find("\"sampler_overrides\":7")!=std::string::npos&&reads>0);
    // Monitoring reports the already decoded control state without another IPC read.
    re5::Settings flags;flags.players[0].analog_enabled=true;flags.players[0].unified_controls=true;
    flags.players[0].gyro_enabled=true;flags.players[0].move_while_aiming=true;
    flags.players[1].move_while_knife=true;flags.players[1].move_while_reloading=true;
    flags.players[1].free_camera=true;flags.players[1].first_person=true;
    flags.players[1].third_person_distance=1.5f;flags.players[1].camera_gain=.75f;flags.players[1].camera_return_speed=.25f;
    assert(re5::encode_settings(flags,live.settings,sizeof(live.settings)));
    std::memcpy(memory.data()+8,&live,sizeof(live));text=diagnose();
    assert(text.find("\"players\":[{\"analog\":true,\"unified\":true,\"gyro\":true,\"move_aim\":true,\"move_knife\":false,\"move_reload\":false,\"free_camera\":false,\"first_person\":false,\"third_person_distance\":1,\"camera_gain\":1,\"camera_return_speed\":1},"
        "{\"analog\":false,\"unified\":false,\"gyro\":false,\"move_aim\":false,\"move_knife\":true,\"move_reload\":true,\"free_camera\":true,\"first_person\":true,\"third_person_distance\":1.5,\"camera_gain\":0.75,\"camera_return_speed\":0.25}]")!=std::string::npos);

    // A supported process becoming unsupported cannot retain its old IPC observation.
    correct_build=false;reads=0;text=diagnose();
    assert(text.find("\"status\":\"unsupported_build\"")!=std::string::npos);
    assert(text.find("\"ipc\":null")!=std::string::npos&&reads==0);
    correct_build=true;text=diagnose();
    // Caller buffers are all-or-empty JSON; invalid arguments perform no IPC at all.
    char tiny[8];std::memset(tiny,'!',sizeof(tiny));const auto calls=process_calls;
    assert(diagnosed.diagnostic_snapshot(nullptr,100)==0);
    assert(diagnosed.diagnostic_snapshot(tiny,0)==0&&tiny[0]=='!');assert(process_calls==calls);
    assert(diagnosed.diagnostic_snapshot(tiny,sizeof(tiny))==0&&tiny[0]==0&&tiny[7]=='!');
    const auto length=diagnose().size();std::vector<char> exact(length+2,'!');
    assert(diagnosed.diagnostic_snapshot(exact.data(),length)==0&&exact[0]==0&&exact[length]=='!');
    assert(diagnosed.diagnostic_snapshot(exact.data(),length+1)==length&&exact[length]==0&&exact[length+1]=='!');
    assert(writes==0);diagnosed.close();
    reset();diagnosed.initialize();memory.fill(0);text=diagnose();
    assert(text.find("\"supported\":true")!=std::string::npos&&text.find("\"ipc\":null")!=std::string::npos);diagnosed.close();
    reset();Re5GameLink link;link.initialize();assert(link.connect());
    assert(link.diagnostics.sampler_overrides==7);
    re5::Settings s;s.graphics.filter=re4::TextureFilter::Aniso8;s.players[0].gyro_speed=1.25f;s.players[1].gyro_speed=3.5f;
    assert(link.apply(s));assert(snapshot().sequence==6);
    assert((sizes==std::vector<std::size_t>{4,re5::kSettingsBytes,4}));
    re5::Settings decoded;const auto c=snapshot();assert(re5::decode_settings(c.settings,sizeof(c.settings),decoded));
    assert(decoded.graphics.filter==re4::TextureFilter::Aniso8&&decoded.players[0].gyro_speed==1.25f&&decoded.players[1].gyro_speed==3.5f);
    ++pid;assert(!link.apply(s)&&writes==3);link.close();
    for(unsigned failure=1;failure<=3;++failure){reset();link.initialize();assert(link.connect());fail_write=failure;
        assert(!link.apply(s)&&writes==failure);assert(failure==1?snapshot().sequence==4:(snapshot().sequence&1));link.close();}
    for(unsigned gate=0;gate<13;++gate){reset();
        if(gate==0)title=1;
        if(gate==1)correct_build=false;
        if(gate==2)attach_result=1;
        if(gate==3)mapping_count=33;
        auto header=snapshot();
        if(gate==4)header.abi=0;
        if(gate==5)header.status=0;
        if(gate==6)header.marker[0]='X';
        if(gate==7)header.sequence=5;
        if(gate==8)header.settings[64]^=1;
        if(gate==9)header.reserved=1;
        if(gate==10)header.capabilities|=512;
        if(gate==11)title=0x010099A00BC1E000ULL;
        if(gate==12)header.applied_sequence=2;
        std::memcpy(memory.data()+8,&header,sizeof(header));link.initialize();assert(!link.connect()&&writes==0);link.close();}
    reset();link.initialize();assert(link.connect());auto header=snapshot();header.capabilities=re5::kKnownCapabilities&~re5::kCapabilityFiltering;
    std::memcpy(memory.data()+8,&header,sizeof(header));assert(!link.apply(s)&&writes==0);link.close();
    reset();link.initialize();assert(link.connect());header=snapshot();header.sequence=5;
    std::memcpy(memory.data()+8,&header,sizeof(header));assert(!link.apply(s)&&writes==0);link.close();
    reset();link.initialize();assert(link.connect());header=snapshot();header.settings[64]^=1;
    std::memcpy(memory.data()+8,&header,sizeof(header));assert(!link.apply(s)&&writes==0);link.close();
    for(unsigned feature=0;feature<9;++feature){
        reset();link.initialize();assert(link.connect());header=snapshot();header.capabilities&=~(1u<<feature);
        std::memcpy(memory.data()+8,&header,sizeof(header));re5::Settings edited;
        if(feature==0)edited.graphics.filter=re4::TextureFilter::Aniso16;
        if(feature==1)edited.graphics.aa=re5::AntiAliasing::Smaa;
        if(feature==2)edited.graphics.draw_distance=1.75f;
        if(feature==3)edited.graphics.shadows=re4::ShadowMode::Off;
        if(feature==4)edited.players[1].gyro_enabled=true;
        if(feature==5)edited.players[1].analog_enabled=true;
        if(feature==6)edited.players[1].move_while_aiming=true;
        if(feature==7)edited.players[1].free_camera=true;
        if(feature==8)edited.players[1].reticle=re4::ReticleStyle::Dot;
        assert(!link.apply(edited)&&writes==0);link.close();
    }
    // A stick-only capable module can accept stick changes without pretending it implements gyro.
    reset();header=snapshot();header.capabilities=re5::kCapabilitySticks;
    std::memcpy(memory.data()+8,&header,sizeof(header));link.initialize();assert(link.connect());
    re5::Settings stick_only;stick_only.players[0].analog_enabled=true;
    assert(link.apply(stick_only)&&writes==3);link.close();
    // Never claim a concurrent producer's newly acknowledged sequence.
    reset();link.initialize();assert(link.connect());header=snapshot();header.sequence+=2;header.applied_sequence=header.sequence;
    std::memcpy(memory.data()+8,&header,sizeof(header));assert(!link.apply(s)&&writes==0);link.close();
    reset();torn=true;link.initialize();assert(!link.connect()&&writes==0);link.close();
    reset();link.initialize();assert(link.connect());torn=true;assert(!link.apply(s)&&writes==0);link.close();
    for(unsigned byte:{0u,15u,16u,31u}){reset();correct_build=false;bad_build_byte=byte;link.initialize();assert(!link.connect()&&writes==0);link.close();}
    // The module also owns a 1024-slot journal preceding this control block.
    // Discovery must not assume the block is in the first128KiB of RW data.
    reset();header=snapshot();memory.fill(0);std::memcpy(memory.data()+0x43000,&header,sizeof(header));
    link.initialize();assert(link.connect());link.close();
    std::puts("RE5 controls IPC: build identity, bounds, diagnostics, transaction and restart guards PASS");
}
