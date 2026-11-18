// Included in game_link.cpp's private namespace. Only an explicit authenticated
// Monitor snapshot invokes this read-only probe; native hooks and guards stay unchanged.
static_assert(re5::profile::legacy_offset(0x1CF8CC8)==0x1CF8CC8);
static_assert(re5::profile::legacy_offset(0x26A9288)==0x26A9288);
struct Re5InputRead {u64 address=0,value=0;unsigned size=0;Result result=0;};
struct Re5InputProbe {
    Service* service;u64 pid;unsigned count=0;bool stable=true;
    std::array<Re5InputRead,8> tail{};
    static bool read(void* context,std::uintptr_t address,void* out,std::size_t size){
        auto& probe=*static_cast<Re5InputProbe*>(context);
        if(!out||!address||!size||size>8||address>UINT64_MAX-size||probe.count>=24)return false;
        if(!re5_same_process(probe.pid)){probe.stable=false;return false;}
        u64 value=0;const auto result=re5_read(probe.service,address,&value,size);
        probe.tail[probe.count++%probe.tail.size()]={address,R_SUCCEEDED(result)?value:0,unsigned(size),result};
        if(!re5_same_process(probe.pid)){probe.stable=false;return false;}
        if(R_FAILED(result))return false;
        std::memcpy(out,&value,size);return true;
    }
};
// One shared budget covers image authentication and both resolved local actors.
// Raw vectors/matrices are memory-order hex bytes, including IEEE754 payloads.
struct Re5ActorProbe {
    static constexpr unsigned max_reads=112,max_bytes=4096;
    Service* service;u64 pid;unsigned count=0,bytes=0,failures=0;bool stable=true,limited=false;
    bool read(u64 base,u64 offset,void* out,std::size_t size){
        if(!stable)return false;
        if(!base||!out||!size||size>128||offset>UINT64_MAX-base||size>UINT64_MAX-(base+offset)){
            ++failures;return false;
        }
        if(count>=max_reads||size>max_bytes-bytes){limited=true;return false;}
        if(!re5_same_process(pid)){stable=false;return false;}
        ++count;bytes+=unsigned(size);const auto result=re5_read(service,base+offset,out,size);
        if(!re5_same_process(pid)){stable=false;return false;}
        if(R_FAILED(result)){++failures;return false;}return true;
    }
    template<class T>bool get(u64 base,u64 offset,T& out){return read(base,offset,&out,sizeof(out));}
};
bool re5_actor_authenticate(Re5ActorProbe& probe,u64 module,u64 main,Re5Build build){
    if(build==Re5Build::None)return false;
    re5::ImageIdentity first{},second{};const auto profile=re5::profile::view(build);
    if(!probe.get(module,sizeof(re5::SharedControl),first)||!probe.get(module,sizeof(re5::SharedControl),second)
        ||std::memcmp(&first,&second,sizeof(first)))return false;
    return !std::memcmp(first.marker,re5::kImageIdentityMarker,16)&&first.abi==1&&first.status==1
        &&!std::memcmp(first.build_id,profile.id->data(),32)
        &&!std::memcmp(first.sdk_build_id,re5::profile::sdk_build_id.data(),32)
        &&first.main_base==main&&first.main_size==re5::profile::main_size&&first.guard_count==profile.count&&!first.reserved;
}
template<class Append>
void re5_actor_state(Re5ActorProbe& probe,u64 main,Re5Build build,u64 actor,Append& append){
    append("{");unsigned fields=0;
    // Emit every attempted field; a failed read is null, never a fabricated zero.
    auto field=[&](const char* name,u64 base,u64 offset,unsigned size,bool raw=false,bool sign=false){
        std::array<u8,128> data{};const bool ok=probe.read(base,offset,data.data(),size);
        append("%s\"%s\":",fields++?",":"",name);
        if(!ok){append("null");return u64(0);}
        u64 value=0;if(size<=8)std::memcpy(&value,data.data(),size);
        if(raw){char hex[257];constexpr char digits[]="0123456789ABCDEF";
            for(unsigned i=0;i<size;++i){hex[2*i]=digits[data[i]>>4];hex[2*i+1]=digits[data[i]&15];}
            hex[2*size]=0;append("\"%s\"",hex);
        }else if(size==8)append("\"%016llX\"",static_cast<unsigned long long>(value));
        else if(sign){const auto bits=size*8;const auto signed_value=std::int64_t(value<<(64-bits))>>(64-bits);
            append("%lld",static_cast<long long>(signed_value));}
        else append("%llu",static_cast<unsigned long long>(value));
        return value;
    };
    field("type",actor,8,2);field("action",actor,0x1B40,4);
    field("evaluator",actor,0x1B10,1);field("frozen",actor,0x1A71,1);
    field("pending",actor,0x1B4C,1);field("scope",actor,0x3AD8,1);
    field("blocked",actor,0x1AB0,1);field("ik",actor,0x168E,1);
    field("native_block",actor,0x2708,1);field("controller_freeze",actor,0x3F0E,1);
    field("hp",actor,0x1EB8,2,false,true);field("max_hp",actor,0x1EBA,2,false,true);
    field("injury",actor,0x29B6,1);field("special",actor,0x29B8,1,false,true);
    field("hand",actor,0x2701,1);field("scene_override",actor,0x29C8,1);
    field("parent",actor,0x48,8);field("rotation_order",actor,0x54,2);field("world_mode",actor,0x56,2);
    field("quaternion_hex",actor,0x70,16,true);
    field("scale_hex",actor,0x80,12,true);field("motion_scale_hex",actor,0x1570,12,true);
    const auto action=field("current_action",actor,0x1B60,8);field("action_owner",action,0x30,8);
    field("weapon_resource",actor,0x15D0,8);field("bone_count",actor,0x8C0,4);
    field("bones",actor,0x8C8,8);field("bone_map",actor,0x8D0,8);
    field("local_matrix_hex",actor,0x90,64,true);field("world_matrix_hex",actor,0xD0,64,true);
    // These static RVAs are identical in both authenticated builds and inside main.
    static_assert(re5::profile::legacy_offset(0x1CD9FC0)==0x1CD9FC0);
    static_assert(re5::profile::legacy_offset(0x1CF7A40)==0x1CF7A40);
    const auto slot=field("scene_slot",main,0x1CD9FC0,8);
    const auto owner=field("scene_owner",slot,0,8);
    const auto context=field("scene_context",owner,0x584E0,8);field("scene_mode",context,0x64,4);
    const auto stage=field("stage",main,0x1CF7A40,8);
    const auto old_failures=probe.failures;
    const auto index=field("stage_index",stage,0x2C,4,false,true);
    if(old_failures==probe.failures&&index<64)field("room",stage,index*0x5F40+0x2AF00,4);
    else {append(",\"room\":null");}
    field("category_table",main,0x1AAEF28,8);
    const auto camera=field("camera",actor,0x1A80,8);
    u64 table=0,camera_owner=0;
    const auto camera_rva=build==Re5Build::Legacy?re5::profile::legacy_offset(0x1AD2510):0x1AD2510;
    const bool qualified=probe.get(camera,0,table)&&table==main+camera_rva
        &&probe.get(camera,0x130,camera_owner)&&camera_owner==actor;
    append(",\"camera_state\":");
    if(qualified){
        append("{");fields=0;
        field("desired_block",camera,0x2EC1,1);field("active",camera,0x198,8);
        field("previous",camera,0x1A0,8);field("default",camera,0xDA0,8);
        field("persistent_hex",camera,0x60,48,true);field("history_hex",camera,0x210,48,true);
        field("produced_hex",camera,0x630,48,true);append("}");
    }else append("null");
    append("}");
}
void re5_input_probe(Service* service,u64 main,u64 pid,char* out,std::size_t capacity,
    Re5Build build=Re5Build::None,u64 module=0){
    if(!out||capacity<5)return;
    std::memcpy(out,"null",5);
    if(!re5_same_process(pid)||main<0x1000||(main&0xfff)||main>UINT64_MAX-re5::profile::main_size)return;
    u64 registry=0;const auto result=re5_read(service,main+0x1CF8CC8,&registry,sizeof(registry));
    if(!re5_same_process(pid))return;
    // The caller supplies per-instance scratch. An incomplete observation must
    // remain JSON null, including process changes after a partial append.
    struct Reset {char* out;bool committed=false;~Reset(){if(!committed)std::memcpy(out,"null",5);}} reset{out};
    char* json=out;std::size_t used=0;
    Re5ActorProbe state{service,pid};unsigned state_players=0;bool authenticated=false,proof_checked=false;
    auto append=[&](const char* format,auto... args){
        if(used>=capacity)return false;
        const auto size=std::snprintf(json+used,capacity-used,format,args...);
        if(size<0||std::size_t(size)>=capacity-used){used=capacity;return false;}
        used+=std::size_t(size);return true;
    };
    append("{\"registry_result\":%u,\"players\":[",unsigned(result));
    unsigned entries=0;
    if(R_SUCCEEDED(result)&&registry&&registry<=UINT64_MAX-0x50)for(unsigned slot=0;slot<4;++slot){
        if(!re5_same_process(pid))return;
        u64 actor=0;const auto actor_result=re5_read(service,registry+0x30+slot*8,&actor,sizeof(actor));
        if(!re5_same_process(pid))return;
        if(R_SUCCEEDED(actor_result)&&!actor)continue;
        Re5InputProbe probe{service,pid};re5::MemoryReader reader{&probe,Re5InputProbe::read,nullptr};
        re5::ResolvedPlayer player;std::uintptr_t wrapper=0;
        const bool resolved=R_SUCCEEDED(actor_result)&&reader.get(actor,0x3EE8,wrapper)&&wrapper&&wrapper<=UINTPTR_MAX-0x350
            &&re5::resolve_player(reader,main,actor,wrapper+0x350,player);
        if(!probe.stable||!re5_same_process(pid))return;
        append("%s{\"slot\":%u,\"actor\":\"%016llX\",\"actor_result\":%u,\"resolved\":%s,\"local_player\":%u,"
            "\"stick_id\":%u,\"stick_style\":%u,\"motion_id\":%u,\"motion_style\":%u,\"read_count\":%u,\"last_reads\":[",
            entries++?",":"",slot,static_cast<unsigned long long>(actor),unsigned(actor_result),resolved?"true":"false",
            player.local_player,player.sticks.controller,player.sticks.style,player.motion.controller,player.motion.style,probe.count);
        const auto retained=std::min<unsigned>(probe.count,probe.tail.size());
        for(unsigned i=0;i<retained;++i){const auto& value=probe.tail[(probe.count-retained+i)%probe.tail.size()];
            append("%s\"%016llX/%u/%016llX/%08X\"",i?",":"",static_cast<unsigned long long>(value.address),
                value.size,static_cast<unsigned long long>(value.value),unsigned(value.result));
        }
        append("],\"actor_state\":");
        if(resolved&&player.local_player<2&&!(state_players&(1u<<player.local_player))){
            if(!proof_checked){
                proof_checked=true;authenticated=re5_actor_authenticate(state,module,main,build);
            }
            if(authenticated){state_players|=1u<<player.local_player;re5_actor_state(state,main,build,actor,append);}
            else append("null");
        }else append("null");
        if(!state.stable||!re5_same_process(pid))return;
        append("}");
    }
    append("],\"actor_state_reads\":%u,\"actor_state_bytes\":%u,\"actor_state_failures\":%u,\"actor_state_limited\":%s}",
        state.count,state.bytes,state.failures,state.limited?"true":"false");
    if(!re5_same_process(pid)||used>=capacity||used>=capacity)return;
    reset.committed=true;
}
