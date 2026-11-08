// Initial CAPCOM/CRIWARE logos are title EFF animations, not movie playback.
// Native4E3B38's substate30 owns fade cleanup, both root-group seeks to400,
// state6 and continuation audio. Request that path; leave its resources native.
HOOK_DEFINE_TRAMPOLINE(StartupLogos){
    static void Callback(void* title){
        if(settings.skip_startup_logos&&title&&main_base){
            EnemyHealthReader reader;std::uintptr_t owner=0;
            const auto address=reinterpret_cast<std::uintptr_t>(title);
            re4::ModernMoveMemory memory;
            if(reader.field(main_base,0x1272A18,owner)&&owner==address
                &&pickup_confirmation_region(address,0x34,true,memory)){
                std::uint8_t stage=0,substate=0;std::int32_t frame=0;
                std::uint64_t edge=0;
                if(memory.value(address,stage)&&memory.value(address+1,substate)
                    &&memory.value(address+0x30,frame)&&stage==4
                    &&(substate==0||substate==1||substate==2||substate==10)
                    &&frame>=105&&frame<=320
                    &&reader.field(main_base,0x124EC58,edge)&&(edge&0x00002000C0001000ull)){
                    // Fresh native accept edges only; do not consume held flags,
                    // alter input or seek an animation/resource from the hook.
                    const std::uint8_t skip=30;
                    std::memcpy(reinterpret_cast<void*>(address+1),&skip,sizeof(skip));
                }
            }
        }
        Orig(title);
    }
};
