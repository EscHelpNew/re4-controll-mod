#include "re4/character_loading.hpp"
// Included after character_appearance.inc; integration supplies its bounded
// memory helpers and installs these two hooks only after native profile guards.
extern "C" std::size_t character_pack_budget_headroom(const void*)
    asm("_ZNK2nn3mem17StandardAllocator18GetAllocatableSizeEv");
struct CharacterPackBudgetNative {
    void (*close)(void*)=nullptr;
    const char* (*prefix)()=nullptr;
    void* (*reserve)(unsigned,const char*,unsigned)=nullptr;
    void (*release)(void*)=nullptr;
    int (*reap)()=nullptr;
    std::size_t (*headroom)(const void*)=nullptr;
} character_pack_budget_native;
struct CharacterPackBudgetScope {
    alignas(8) std::uint8_t file[0x60]{};
    char path[128]{},yz2[132]{};
    std::uint64_t thread=0;
    unsigned bytes=0;
    void* reservation=nullptr;
    bool opened=false,handed=false,consumed=false;
};
std::atomic_flag character_pack_budget_busy=ATOMIC_FLAG_INIT;
std::atomic<std::uint64_t> character_pack_budget_thread{0};
CharacterPackBudgetScope* character_pack_budget_scope=nullptr;
CharacterPackBudgetScope* character_pack_budget_current(){
    const auto owner=character_pack_budget_thread.load(std::memory_order_acquire);
    if(!owner)return nullptr;
    u64 current=0;
    if(R_FAILED(svcGetThreadId(&current,CUR_THREAD_HANDLE))||current!=owner)return nullptr;
    return character_pack_budget_scope;
}
HOOK_DEFINE_TRAMPOLINE(CharacterPackOpen) {
    static int Callback(const char* path,void* info){
        auto* scope=character_pack_budget_current();
        if(!scope)return Orig(path,info);
        if(!path||!info||std::strcmp(path,scope->path)||!scope->opened||scope->handed)return 0;
        // Reproduce ONLY the fields written by 254EE0. In particular preserve
        // the loader's registry/callback association at FileInfo+40.
        character_store(info,0x20,character_value<std::uintptr_t>(scope->file,0x20));
        character_store(info,0x1C,character_value<unsigned>(scope->file,0x1C));
        character_store(info,0x50,scope->bytes);
        scope->opened=false;scope->handed=true;return 1;
    }
};
HOOK_DEFINE_TRAMPOLINE(CharacterPackAllocate) {
    static void* Callback(unsigned bytes,unsigned alignment){
        auto* scope=character_pack_budget_current();
        if(!scope||!scope->handed||scope->consumed||bytes!=scope->bytes||(alignment&255)!=128)
            return Orig(bytes,alignment);
        // Match 255B00's deliberately NONZERO prefix, including 128 when the
        // StandardAllocator base is already aligned. Native 255B60 owns release.
        auto* base=static_cast<std::uint8_t*>(scope->reservation);
        if(!base)return nullptr;
        const unsigned offset=128-(reinterpret_cast<std::uintptr_t>(base)&127);
        auto* result=base+offset;result[-1]=std::uint8_t(offset);
        scope->reservation=nullptr;scope->consumed=true;return result;
    }
};
bool character_pack_budget_io_idle(bool& retry){
    // No previous native IO child may remain active. Native 255060 safely
    // reaps finished handles; a not-yet-exited thread still means retry.
    for(unsigned i=0;i<20;++i){
        unsigned active=0;
        if(!character_read(main_base+0x1184300+i*0x58+0x50,active)||active>1)return false;
        if(active){retry=true;return false;}
    }
    character_pack_budget_native.reap();
    for(unsigned i=0;i<20;++i){
        std::uintptr_t thread=0;
        if(!character_read(main_base+0x11852A0+i*8,thread))return false;
        if(thread){retry=true;return false;}
    }
    return true;
}
bool character_pack_budget_mapping(unsigned file,unsigned pack,unsigned& mode){
    std::uint8_t initialized=0;unsigned mapped=0;std::uintptr_t workspace=0;
    // Only the seven fixed installed donor resources can use this adapter.
    const bool allowed=(file==0x22&&pack==0x01000001)||(file==0xA8&&pack==0x0100000B)
        ||(file==0x49&&pack==0x01000014)||(file==0xC5&&pack==0x0100000A)
        ||(file==0x34&&pack==0x18)||(file==0x64&&pack==0x34)||(file==0x18&&pack==0x10);
    return allowed&&character_read(main_base+0x11807D4,initialized)&&(initialized&1)
        &&character_read(main_base+0x11807D8+file*4,mapped)&&mapped==pack
        &&character_read(main_base+0x1180D40+0x158,workspace)&&workspace
        &&character_read(main_base+0xAF65B0,mode);
}
bool character_pack_budget_paths(CharacterPackBudgetScope& scope,unsigned mode,unsigned pack){
    const auto* prefix=character_pack_budget_native.prefix();
    // 255BC0 is a guarded pure return of the five-byte "bio4" literal. Native
    // 24DAC0 selects ImagePack for mode0, ImagePackHD for ANY nonzero mode.
    if(!prefix||std::memcmp(prefix,"bio4",5))return false;
    const char* directory=mode?"bio4\\ImagePackHD\\":"bio4\\ImagePack\\";
    const auto length=std::strlen(directory);std::memcpy(scope.path,directory,length);
    constexpr char hex[]="0123456789abcdef";
    for(unsigned i=0;i<8;++i)scope.path[length+i]=hex[(pack>>(28-4*i))&15];
    std::memcpy(scope.path+length+8,".pack",6);
    std::memcpy(scope.yz2,scope.path,length+13);std::memcpy(scope.yz2+length+13,".yz2",5);
    return true;
}
bool character_pack_budget_load(void* context,unsigned file,unsigned pack,
        void(*native_load)(void*,unsigned,unsigned),unsigned options,bool* retry_out,std::uint32_t* diagnostic_out=nullptr){
    using S=re4::CharacterLoadingStage;
    if(diagnostic_out)*diagnostic_out=0;
    const auto reject=[&](S stage,unsigned observed=0){if(diagnostic_out)*diagnostic_out=re4::character_loading_diagnostic(stage,observed);return false;};
    bool retry=false;if(retry_out)*retry_out=false;
    if(character_pack_budget_busy.test_and_set(std::memory_order_acquire)){
        if(retry_out)*retry_out=true;
        return false;
    }
    struct Unlock {~Unlock(){character_pack_budget_busy.clear(std::memory_order_release);}} unlock;
    if(!context||!native_load||!character_pack_budget_native.close||!character_pack_budget_native.prefix
        ||!character_pack_budget_native.reserve||!character_pack_budget_native.release||!character_pack_budget_native.reap)return reject(S::Context);
    unsigned mode=0;if(!character_pack_budget_mapping(file,pack,mode))return reject(S::Mapping);
    bool capacity=false;
    for(unsigned i=0;i<64;++i){
        const auto entry=main_base+0x11829B0+i*16;
        unsigned id=0;if(!character_read(entry+8,id))return reject(S::ResidentEntry);
        if(id==pack){
            std::uintptr_t blob=0;std::uint8_t ready=0;
            return character_read(entry,blob)&&blob&&character_read(entry+15,ready)&&ready<=1?true:reject(S::ResidentEntry);
        }
        capacity|=id==0xffffffffu;
    }
    if(!capacity)return reject(S::RegistryCapacity);
    if(!character_pack_budget_io_idle(retry)){if(retry_out)*retry_out=retry;return retry?false:reject(S::IoState);}
    CharacterPackBudgetScope scope;
    struct Cleanup {
        CharacterPackBudgetScope& scope;
        ~Cleanup(){
            character_pack_budget_thread.store(0,std::memory_order_release);character_pack_budget_scope=nullptr;
            if(scope.opened)character_pack_budget_native.close(scope.file);
            if(scope.reservation)character_pack_budget_native.release(scope.reservation);
        }
    } cleanup{scope};
    if(R_FAILED(svcGetThreadId(&scope.thread,CUR_THREAD_HANDLE))||!scope.thread)return reject(S::Thread);
    if(!character_pack_budget_paths(scope,mode,pack))return reject(S::Prefix);
    // A present YZ2 takes a second allocation/copy on a separate native IO
    // thread. Refuse that variant BEFORE entering 24DA70 or touching its errors.
    if(CharacterPackOpen::Orig(scope.yz2,scope.file)){
        scope.opened=true;return reject(S::CompressedVariant);
    }
    if(!CharacterPackOpen::Orig(scope.path,scope.file))return reject(S::FileOpen);
    scope.opened=true;scope.bytes=character_value<unsigned>(scope.file,0x50);
    if(!character_value<std::uintptr_t>(scope.file,0x20)||!scope.bytes
        ||scope.bytes>std::numeric_limits<unsigned>::max()-128)return reject(S::FileSize);
    scope.reservation=character_pack_budget_native.reserve(scope.bytes+128,"RE4 cosmetic texture pack",1);
    if(!scope.reservation)return reject(S::Allocation);
    // A retained seven-pack cache is bounded, but can still consume 196 MiB of
    // CPU heap alone. After our real reservation, preserve one conservative
    // contiguous 128 MiB block for subsequent native allocations. This is a
    // policy margin, not a guarantee for every future engine allocation.
    std::uintptr_t allocator=0;
    if(!character_pack_budget_native.headroom||!character_read(main_base+0xB27538,allocator)||!allocator)return reject(S::Heap);
    const auto headroom=character_pack_budget_native.headroom(reinterpret_cast<const void*>(allocator));
    if(headroom<std::size_t(128)*1024*1024)return reject(S::Headroom,re4::character_loading_mebibytes(headroom));
    // All potentially blocking file/allocator work is complete. Revalidate the
    // live mapping/mode and the native IO slot immediately before native load.
    unsigned current_mode=0;
    if(!character_pack_budget_mapping(file,pack,current_mode)||current_mode!=mode)return reject(S::Revalidation);
    if(!character_pack_budget_io_idle(retry)){if(retry_out)*retry_out=retry;return retry?false:reject(S::IoState);}
    character_pack_budget_scope=&scope;
    character_pack_budget_thread.store(scope.thread,std::memory_order_release);
    native_load(context,file,options);
    return scope.handed&&scope.consumed?true:reject(S::Handshake);
}
[[maybe_unused]] bool character_pack_budget_load(void* context,unsigned file,unsigned pack,
        void(*native_load)(void*,unsigned,unsigned),unsigned options){
    return character_pack_budget_load(context,file,pack,native_load,options,nullptr);
}
void character_pack_budget_install(){
    character_pack_budget_native.close=reinterpret_cast<decltype(character_pack_budget_native.close)>(main_base+0x254ED0);
    character_pack_budget_native.prefix=reinterpret_cast<decltype(character_pack_budget_native.prefix)>(main_base+0x255BC0);
    character_pack_budget_native.reserve=reinterpret_cast<decltype(character_pack_budget_native.reserve)>(main_base+0x282040);
    character_pack_budget_native.release=reinterpret_cast<decltype(character_pack_budget_native.release)>(main_base+0x282180);
    character_pack_budget_native.reap=reinterpret_cast<decltype(character_pack_budget_native.reap)>(main_base+0x255060);
    character_pack_budget_native.headroom=character_pack_budget_headroom;
    CharacterPackOpen::InstallAtOffset(0x254EE0);
    CharacterPackAllocate::InstallAtOffset(0x255B00);
}
