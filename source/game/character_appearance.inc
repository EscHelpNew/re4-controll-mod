// Build-specific render adapter. The real actor, skeleton and native chain are
// read-only. All native relocation and all temporary next links target our banks.
enum class CharacterPhase : unsigned {Free,Reading,Probe,Reservations,CancelReservations,UploadProbe,Active,Invalidating,Retiring};
bool character_pack_budget_load(void*,unsigned,unsigned,void(*)(void*,unsigned,unsigned),unsigned,bool*,std::uint32_t*);
constexpr std::size_t character_bank_size=9*1024*1024;
constexpr std::size_t character_deformed_slot=re4::character_asset_deformed_slot_limit; // Three slots fit in 2 MiB.
constexpr unsigned character_native_blade_vertices=163;
constexpr std::size_t character_native_blade_array=1984,character_native_blade_bytes=3968;
struct CharacterAttachedSource {
    std::uintptr_t part=0,bin=0,tpl=0,archive=0;
    unsigned flags=0;std::uint8_t alpha=0;
    re4::ViewMatrix3x4 matrix{};
    bool operator==(const CharacterAttachedSource& other)const {
        return part==other.part&&bin==other.bin&&tpl==other.tpl&&archive==other.archive
            &&flags==other.flags&&alpha==other.alpha&&!std::memcmp(&matrix,&other.matrix,sizeof(matrix));
    }
};
struct CharacterNative {
    void* (*effect_script)(unsigned,unsigned,unsigned)=nullptr;
    bool (*initialize)(void*,void*,unsigned)=nullptr;
    void (*attach)(void*,void*,void*)=nullptr;
    void* (*output)()=nullptr;
    void (*positions)(void*,const void*,const void*,unsigned)=nullptr;
    void (*normals)(void*,const void*,const void*,unsigned)=nullptr;
    void (*texture_object)(void*,const void*,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned)=nullptr;
    void (*load_texture)(void*,unsigned,unsigned)=nullptr;
    void* (*command)(std::uint64_t)=nullptr;
    void* (*context)(unsigned)=nullptr;
    void (*finish)(void*)=nullptr;
    void (*flush)()=nullptr;
    void (*wait_thread)(void*)=nullptr;
    void (*reset_cache)(void*)=nullptr;
    void (*cleanup)()=nullptr;
} character_native;
struct CharacterIdentity {
    std::uintptr_t actor=0,chain=0,bin=0,bones=0,asset=0,bindings=0,state=0;
    std::uint16_t room=0;std::uint64_t renderer=0,context=0;
    bool operator==(const CharacterIdentity& b)const {
        return actor==b.actor&&chain==b.chain&&bin==b.bin&&bones==b.bones
            &&asset==b.asset&&bindings==b.bindings&&state==b.state&&room==b.room&&renderer==b.renderer&&context==b.context;
    }
};
struct CharacterEmitterCache {
    CharacterIdentity identity{};
    std::uint64_t token=0;
    std::uintptr_t bones[26]{};
    re4::ViewMatrix3x4 world[26]{},inverse_bind[26]{};
    re4::CharacterEmitterPose pose{};
    bool valid=false;
};

// Keep large zeroed storage separate from nonzero bind-table defaults, so it is
// true BSS rather than tens of MiB of initialized zeros in the module file.
// Audited maximum SUM of all BIN position/normal arrays in one installed
// wep*.udas is129344 bytes (wep17, five meshes). Admission rejects duplicate
// BINs and capacity overflow; each context/handle/frame owns its own copy.
constexpr std::size_t character_held_geometry_capacity=129344;
struct CharacterStorage {
    alignas(32) std::uint8_t raw[character_bank_size];
    alignas(32) std::uint8_t parts[re4::character_asset_piece_limit][0x1A8];
    alignas(32) std::uint8_t deformed[3][character_deformed_slot];
    alignas(32) std::uint8_t held[2][3][2][character_held_geometry_capacity];
} character_storage[2];
constexpr unsigned character_held_piece_limit=8;
struct CharacterHeldFrame {
    std::uintptr_t actor=0,chain=0,root=0,parent=0;
    unsigned count=0;bool captured=false,drawn=false;
    std::uintptr_t archive=0,source_parts[character_held_piece_limit]{},base_bones[character_held_piece_limit]{};
    std::size_t geometry_used=0,geometry_offset[character_held_piece_limit]{};
    unsigned vertices[character_held_piece_limit]{},normals[character_held_piece_limit]{};
    bool weighted[character_held_piece_limit]{};
    alignas(32) std::uint8_t original[character_held_piece_limit][0x1A8]{};
    alignas(32) std::uint8_t parts[character_held_piece_limit][0x1A8]{};
};
struct CharacterVisualFrame {
    re4::ViewVec3 shift[3]{}; // Native parent10 /16 /17, in player-root space.
    re4::ViewMatrix3x4 knife_half_turn{};
    bool valid=false,knife_valid=false;
    CharacterHeldFrame held[2]{};
};
struct CharacterHeldSkin {
    std::uintptr_t actor=0,chain=0,root=0,parent=0,archive=0;
    std::uint64_t frame=0;bool valid=false;
};
struct CharacterBinding {
    std::uint8_t (*parts)[0x1A8]=nullptr;
    std::uint8_t (*deformed)[character_deformed_slot]=nullptr;
    std::size_t capacity=character_deformed_slot;
    CharacterIdentity identity{};
    re4::CharacterRetargetMap map{};
    std::atomic<std::uint64_t> skinned[3]{},slot_token[3]{},slot_ack[3]{};
    CharacterVisualFrame visual[3]{};
    std::uint8_t (*held_geometry)[2][character_held_geometry_capacity]=nullptr;
    CharacterHeldSkin held_skin[3][2]{};
    bool prepared=false;
};
struct CharacterHeightCache {
    std::atomic<std::uintptr_t> actor{0},chain{0},bin{0},bones{0},asset{0},bindings{0},state{0};
    std::atomic<std::uint64_t> renderer{0},token{0};
    std::atomic<unsigned> room{0},bits{0},appearance{0};
};
struct CharacterBank : CharacterBinding {
    CharacterEmitterCache emitter{};
    CharacterHeightCache height{};
    std::uint8_t* raw=nullptr;
    CharacterBinding menu{};
    alignas(32) std::uint8_t menu_parts[re4::character_asset_piece_limit][0x1A8]{};
    // Independent of the seven donor recipe slots (Ashley uses all seven).
    alignas(32) std::uint8_t attached_parts[3][0x1A8]{};
    re4::CharacterKnifeOrientation knife_orientation{};
    bool knife_orientation_valid=false;
    CharacterAttachedSource attached_source[3]{};
    bool attached_valid[3]{};
    std::atomic<CharacterPhase> phase{CharacterPhase::Free};
    std::atomic<std::uint64_t> ack{0},token{0};
    std::atomic<unsigned> pack{0};
    std::atomic<bool> pinned{false};
    std::atomic<unsigned> load_result{0};
    std::atomic<unsigned> fault{0};
    std::atomic<std::uint32_t> load_diagnostic{0};
    std::atomic<std::uint64_t> drawn{0};
    std::atomic<re4::CharacterAppearanceError> draw_wait{re4::CharacterAppearanceError::WaitingSkin};
    re4::CharacterAppearance appearance=re4::CharacterAppearance::Leon;
    re4::CharacterAssetReady ready{};
    nn::fs::FileHandle file{};
    bool opened=false;
    std::size_t read=0;
};
CharacterBank character_banks[2];
std::atomic<std::uint64_t> character_menu_generation{1};
std::atomic_flag character_render_lock=ATOMIC_FLAG_INIT;
std::atomic<bool> character_teardown{false};
std::atomic<std::uint64_t> character_serial{0};
std::atomic<std::uint64_t> character_renderer_generation{1};
std::atomic<bool> character_renderer_ready{false};
char character_mount[16]{};
std::atomic<bool> character_mount_ready{false};
re4::CharacterAppearance character_desired=re4::CharacterAppearance::Leon;
re4::CharacterAppearanceError character_error=re4::CharacterAppearanceError::None;

template<class T> bool character_read(std::uintptr_t address,T& out){return modern_move_read(address,out);}
bool character_region(std::uintptr_t address,std::size_t size){
    re4::ModernMoveMemory memory;return modern_move_region(address,size,false,memory);
}
struct CharacterMemoryReader {
    template<class T> bool value(std::uintptr_t p,T& out){return character_read(p,out);}
    bool region(std::uintptr_t p,std::size_t bytes){return character_region(p,bytes);}
};
// Only permission/range proofs are reused, for one synchronous callback. Field
// bytes are freshly read on every value()/descriptor check; no native owner,
// source geometry or live pose authorization survives the callback.
struct CharacterEmitterReader {
    re4::ModernMoveMemory regions[32]{};unsigned count=0;
    bool view(std::uintptr_t p,std::size_t bytes,re4::ModernMoveMemory& out){
        for(unsigned i=0;i<count;++i)if(regions[i].contains(p,bytes)){out=regions[i];return true;}
        if(count==32)return false;
        re4::ModernMoveMemory memory;
        if(!modern_move_region(p,bytes,false,memory))return false;
        regions[count++]=memory;out=memory;return true;
    }
    bool region(std::uintptr_t p,std::size_t bytes){re4::ModernMoveMemory m;return view(p,bytes,m);}
    template<class T> bool value(std::uintptr_t p,T& out){re4::ModernMoveMemory m;return view(p,sizeof(T),m)&&m.value(p,out);}
};

template<class T> T character_value(const void* p,std::size_t offset){
    T value{};std::memcpy(&value,static_cast<const std::uint8_t*>(p)+offset,sizeof(value));return value;
}
template<class T> void character_store(void* p,std::size_t offset,T value){
    std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));
}
void character_appearance_ack(std::atomic<std::uint64_t>& ack,std::uint64_t token){
    auto before=ack.load(std::memory_order_acquire);
    while(before<token&&!ack.compare_exchange_weak(before,token,std::memory_order_release,std::memory_order_acquire)){}
}
bool character_texture_ready(unsigned pack);
void character_appearance_publish(re4::CharacterAppearance applied,re4::CharacterAppearanceStatus status,
        re4::CharacterAppearanceError waiting=re4::CharacterAppearanceError::None){
    const auto sequence=(control.appearance_generation&~1u)+2;
    control.appearance_generation=sequence-1;std::atomic_thread_fence(std::memory_order_release);
    control.appearance_applied=unsigned(applied);control.appearance_status=unsigned(status);
    control.appearance_error=unsigned(status==re4::CharacterAppearanceStatus::Pending?waiting:character_error);
    std::atomic_thread_fence(std::memory_order_release);control.appearance_generation=sequence;
}
void character_appearance_mount(const char* name){
    if(character_mount_ready.load(std::memory_order_acquire)||!name)return;
    unsigned n=0;while(n<sizeof(character_mount)-1&&name[n]){
        const char c=name[n];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return;
        ++n;
    }
    if(!n||name[n])return;
    std::memcpy(character_mount,name,n+1);character_mount_ready.store(true,std::memory_order_release);
}
void character_appearance_request_retirement(CharacterBank& b){
    b.height.appearance.store(0,std::memory_order_release);
    b.height.token.store(0,std::memory_order_release);
    auto phase=b.phase.load(std::memory_order_acquire);
    if(phase==CharacterPhase::Reservations){
        b.phase.compare_exchange_strong(phase,CharacterPhase::CancelReservations,std::memory_order_acq_rel);return;
    }
    while(phase==CharacterPhase::Active||phase==CharacterPhase::Probe||phase==CharacterPhase::UploadProbe){
        if(!b.phase.compare_exchange_weak(phase,CharacterPhase::Invalidating,std::memory_order_acq_rel))continue;
        b.token.store(character_serial.fetch_add(1,std::memory_order_acq_rel)+1,std::memory_order_release);
        b.phase.store(CharacterPhase::Retiring,std::memory_order_release);return;
    }
}
void character_appearance_retire(CharacterBank& b){
    const auto phase=b.phase.load(std::memory_order_acquire);
    if(phase==CharacterPhase::Free||phase==CharacterPhase::Retiring)return;
    if(b.opened){nn::fs::CloseFile(b.file);b.opened=false;}
    if(phase==CharacterPhase::Reading){b.phase.store(CharacterPhase::Free,std::memory_order_release);return;}
    character_appearance_request_retirement(b);
}
void character_appearance_collect(){
    for(auto& b:character_banks)if(b.phase.load(std::memory_order_acquire)==CharacterPhase::Retiring
        &&b.ack.load(std::memory_order_acquire)>=b.token.load(std::memory_order_acquire)
        &&(b.load_result.load(std::memory_order_acquire)!=1||character_texture_ready(b.pack.load(std::memory_order_relaxed)))){
        b.pinned.store(false,std::memory_order_release);b.phase.store(CharacterPhase::Free,std::memory_order_release);
    }
}
void character_appearance_reset(){
    // Initialization/test reset only. Runtime release always goes through collect.
    unsigned index=0;
    for(auto& b:character_banks){
        b.raw=character_storage[index].raw;b.parts=character_storage[index].parts;b.deformed=character_storage[index].deformed;
        b.held_geometry=character_storage[index].held[0];b.menu.held_geometry=character_storage[index].held[1];++index;
        if(b.opened)nn::fs::CloseFile(b.file);
        b.opened=false;b.pinned.store(false);b.phase.store(CharacterPhase::Free);b.ack.store(0);b.token.store(0);
        b.menu.parts=b.menu_parts;b.menu.deformed=nullptr;b.menu.identity={};b.menu.prepared=false;b.prepared=false;
        b.height.appearance.store(0,std::memory_order_release);b.height.token.store(0,std::memory_order_release);
        b.knife_orientation_valid=false;b.emitter.valid=false;
        for(auto& slot:b.held_skin)for(auto& h:slot)h={};
        for(auto& slot:b.menu.held_skin)for(auto& h:slot)h={};
        for(auto& v:b.visual){v.valid=v.knife_valid=false;for(auto& h:v.held)h.captured=false;}
        for(auto& v:b.menu.visual){v.valid=v.knife_valid=false;for(auto& h:v.held)h.captured=false;}
        for(auto& valid:b.attached_valid)valid=false;
        for(auto& slot:b.menu.skinned)slot.store(0);
        for(auto& slot:b.menu.slot_token)slot.store(0);
        for(auto& slot:b.menu.slot_ack)slot.store(0);
        for(auto& slot:b.skinned)slot.store(0);
        for(auto& slot:b.slot_token)slot.store(0);
        for(auto& slot:b.slot_ack)slot.store(0);
        b.drawn.store(0);b.draw_wait.store(re4::CharacterAppearanceError::WaitingSkin);b.load_result.store(0);b.fault.store(0);b.load_diagnostic.store(0);
    }
    character_native={};character_mount[0]=0;character_mount_ready.store(false);
    character_desired=re4::CharacterAppearance::Leon;character_error=re4::CharacterAppearanceError::None;
    character_teardown.store(false);character_renderer_ready.store(false);character_render_lock.clear();
}
template<class Reader> bool character_effect_supported_read(std::uintptr_t effect,unsigned flags,Reader& reader){
    if(!effect||!(flags&0x10))return true;
    // Ordinary player constructors install this native bone-effect table.
    // 4EDD64..78 invokes21B0F0 once with the REAL actor after mesh traversal;
    // 21B460..4D4 uses this table and actor108 bones, never the donor chain.
    // Preserve that native call, owner and flags. Unknown owners stay blocked.
    unsigned count=0;std::uintptr_t table=0;
    return effect==main_base+0xAF4798&&reader.value(effect,count)&&count==6
        &&reader.value(effect+8,table)&&table==main_base+0xAF4768&&reader.region(table,6*8);
}
bool character_effect_supported(std::uintptr_t effect,unsigned flags){CharacterMemoryReader reader;return character_effect_supported_read(effect,flags,reader);}
template<class Reader> bool character_identity_read(void* actor,CharacterIdentity& out,Reader& reader,re4::CharacterAppearanceError* waiting=nullptr){
    using E=re4::CharacterAppearanceError;
    const auto reject=[&](E reason){if(waiting)*waiting=reason;return false;};
    const auto p=reinterpret_cast<std::uintptr_t>(actor);
    if(!main_base||!character_renderer_ready.load(std::memory_order_acquire)
        ||character_teardown.load(std::memory_order_acquire))return reject(E::WaitingRenderer);
    if(!p||p!=modern_move_slot(re4::profile::player_slot)||!reader.region(p,0xA00))return reject(E::WaitingPlayer);
    // Cosmetic ownership is independent of input, camera mode and action state.
    std::uint8_t type=0,count=0;std::uintptr_t alternate=0,remap=0,effect=0;
    std::uint32_t flags=0;
    CharacterIdentity value;value.actor=p;value.state=modern_move_slot(0xB1AE08);
    value.renderer=character_renderer_generation.load(std::memory_order_acquire);
    if(!value.state||!reader.region(value.state,0x50A0))return reject(E::WaitingState);
    if(!reader.value(p+0x118,type))return reject(E::WaitingModel);
    if(type)return reject(re4::character_appearance_diagnostic(E::WaitingModel,type));
    if(!reader.value(p+0x11A,count))return reject(E::WaitingBones);
    if(count!=119)return reject(re4::character_appearance_diagnostic(E::WaitingBones,count));
    if(!reader.value(p+0x180,value.chain)||!value.chain||!reader.value(value.chain+0x18,value.bin)||!value.bin)return reject(E::WaitingChain);
    if(!reader.value(p+0x108,value.bones)||!value.bones)return reject(E::WaitingBones);
    if(!reader.value(p+0x428,value.asset)||!reader.value(p+0x548,value.bindings))return reject(E::WaitingChain);
    if(!reader.value(p+0x188,alternate)||alternate)return reject(E::WaitingAlternate);
    if(!reader.value(p+0x3B8,remap)||remap)return reject(E::WaitingMaterial);
    if(!reader.value(p+0x3A0,effect)||!reader.value(p+8,flags)||!character_effect_supported_read(effect,flags,reader))return reject(E::WaitingEffect);
    if(!reader.value(value.state+0x5020,value.room))return reject(E::WaitingState);
    std::uint8_t native_type=255,bone_count=0;
    if(!reader.value(value.state+0x503C,native_type)||native_type)return reject(E::WaitingCharacter);
    if(!reader.value(value.bin+0x29,bone_count))return reject(E::WaitingBinBones);
    if(bone_count!=119)return reject(re4::character_appearance_diagnostic(E::WaitingBinBones,bone_count));
    if(waiting)*waiting=E::None;
    out=value;return true;
}
bool character_identity(void* actor,CharacterIdentity& out,re4::CharacterAppearanceError* waiting=nullptr){CharacterMemoryReader reader;return character_identity_read(actor,out,reader,waiting);}
bool character_appearance_prepare_binding(CharacterBank& bank,CharacterBinding& b,const CharacterIdentity& identity){
    const auto& ready=bank.ready;
    if(!ready.piece_count||ready.piece_count>re4::character_asset_piece_limit
        ||ready.pieces[0].role!=re4::CharacterAssetRole::Body)return false;
    std::uintptr_t bind=0;
    if(!character_read(identity.bin,bind)||!character_region(bind,119*16))return false;
    re4::CharacterBindBone source[119];
    for(unsigned i=0;i<119;++i){
        const auto* row=reinterpret_cast<const std::uint8_t*>(bind+i*16);
        source[i].id=row[0];source[i].parent=row[1];std::memcpy(&source[i].position,row+4,12);
    }
    if(!re4::character_retarget_map(source,119,ready.body_bind.data(),ready.body_bone_count,b.map)
        ||!character_native.initialize||!character_native.attach)return false;
    std::size_t used=0;void* tail=nullptr;bool knife=false;
    for(unsigned i=0;i<ready.piece_count;++i){
        auto* part=b.parts[i];auto& piece=ready.pieces[i];std::memset(part,0,0x1A8);
        const std::size_t positions=(std::size_t(piece.vertex_count)*12+31)&~std::size_t(31);
        const std::size_t normals=(std::size_t(piece.normal_count)*12+31)&~std::size_t(31);
        if(used>b.capacity||positions>b.capacity-used
            ||normals>b.capacity-used-positions)return false;
        if(!character_native.initialize(nullptr,part,0))return false;
        character_native.attach(part,piece.bin.data,piece.tpl.data);
        auto flags=character_value<std::uint32_t>(part,8);
        flags=(flags|piece.flags_or)&~piece.flags_clear;
        // A validated weighted one-bone piece uses the native zero-target morph
        // path to force skinning; all five morph pointers remain null.
        if(piece.bone_record_count==1)flags|=2;
        character_store(part,8,flags);
        character_store(part,0x28,static_cast<void*>(nullptr));
        if(piece.role==re4::CharacterAssetRole::Knife){
            if(knife)return false;
            knife=true; // Separate native weapon actor owns this visual piece.
        }else {
            if(piece.role!=re4::CharacterAssetRole::Body&&piece.role!=re4::CharacterAssetRole::RightHand
                &&piece.role!=re4::CharacterAssetRole::LeftHand)return false;
            if(tail)character_store(tail,0x28,static_cast<void*>(part));
            tail=part;
        }
        for(unsigned slot=0;slot<3;++slot){
            character_store(part,0xB0+slot*8,static_cast<void*>(b.deformed[slot]+used));
            character_store(part,0xC8+slot*8,static_cast<void*>(b.deformed[slot]+used+positions));
        }
        used+=positions+normals;
    }
    b.identity=identity;b.prepared=true;
    if(&b==static_cast<CharacterBinding*>(&bank))bank.emitter.valid=false;
    if(&b==static_cast<CharacterBinding*>(&bank)){
        bank.height.appearance.store(0,std::memory_order_release);bank.height.token.store(0,std::memory_order_release);
    }
    for(auto& slot:b.held_skin)for(auto& h:slot)h={};
    for(auto& v:b.visual){v.valid=v.knife_valid=false;for(auto& h:v.held)h.captured=false;}
    if(&b==static_cast<CharacterBinding*>(&bank))for(auto& valid:bank.attached_valid)valid=false;
    for(auto& slot:b.skinned)slot.store(0,std::memory_order_relaxed);
    for(auto& slot:b.slot_token)slot.store(0,std::memory_order_relaxed);
    for(auto& slot:b.slot_ack)slot.store(0,std::memory_order_relaxed);
    return true;
}
bool character_binding_idle(const CharacterBinding& b){
    for(unsigned slot=0;slot<3;++slot)if(b.slot_ack[slot].load(std::memory_order_acquire)<b.slot_token[slot].load(std::memory_order_acquire))return false;
    return true;
}
void character_menu_invalidate(){
    // May be requested without the render lock. Touch no binding or borrowed
    // model memory here; producer callbacks observe the new generation and
    // rebuild only under the lock after every old slot consumer acknowledges.
    character_menu_generation.fetch_add(1,std::memory_order_acq_rel);
}
bool character_appearance_prepare(CharacterBank& b,const CharacterIdentity& identity){
    // Prepare from relative authenticated bytes before the first native attach.
    b.knife_orientation_valid=false;
    if(b.appearance==re4::CharacterAppearance::Krauser)
        for(unsigned i=0;i<b.ready.piece_count;++i)if(b.ready.pieces[i].role==re4::CharacterAssetRole::Knife){
            const auto bin=b.ready.pieces[i].bin;
            b.knife_orientation_valid=re4::character_knife_prepare({bin.data,bin.size},
                b.ready.body_bind.data(),b.ready.body_bone_count,b.knife_orientation);
            break;
        }
    const auto* recipe=re4::character_asset_recipe(b.appearance);if(!recipe)return false;
    // Only bytes beyond the authenticated resource may be used as scratch.
    const auto tail=(recipe->size+31)&~std::size_t(31);
    b.capacity=character_deformed_slot;
    if(tail<=character_bank_size&&3*character_deformed_slot<=character_bank_size-tail){
        b.menu.deformed=reinterpret_cast<std::uint8_t (*)[character_deformed_slot]>(b.raw+tail);
        b.menu.capacity=character_deformed_slot;
    }else {
        // Largest archive has tiny geometry. Partition each existing slot.
        b.capacity=character_deformed_slot/2;b.menu.capacity=b.capacity;
        b.menu.deformed=reinterpret_cast<std::uint8_t (*)[character_deformed_slot]>(b.deformed[0]+b.capacity);
    }
    b.menu.identity={};b.menu.prepared=false;
    return character_appearance_prepare_binding(b,b,identity);
}
bool character_menu_identity(void* actor,CharacterIdentity& out){
    const auto p=reinterpret_cast<std::uintptr_t>(actor),pool=main_base+0xAFF4A8;
    if(!main_base||!character_renderer_ready.load(std::memory_order_acquire)||character_teardown.load(std::memory_order_acquire))return false;
    std::uintptr_t base=0,preview=0,vptr=0;unsigned count=0,stride=0,flags=0;
    if(!character_read(pool+8,base)||!base||!character_read(main_base+0x1272140,preview)||p!=preview||p!=base
        ||!character_read(pool+0x10,count)||!count||count>161||!character_read(pool+0x14,stride)||stride!=0x3C8
        ||!character_region(base,std::size_t(count)*stride)||!character_read(p,vptr)||vptr!=main_base+0xAFF538
        ||!character_read(p+8,flags)||(flags&0x201)!=1)return false;
    CharacterIdentity value;value.actor=p;value.state=modern_move_slot(0xB1AE08);value.context=character_menu_generation.load(std::memory_order_acquire);
    value.renderer=character_renderer_generation.load(std::memory_order_acquire);
    std::uint8_t type=255,bones=0,native=255;std::uintptr_t alternate=0,remap=0,effect=0;
    if(!value.state||!character_region(value.state,0x50A0)||!character_read(value.state+0x503C,native)||native
        ||!character_read(p+0x118,type)||type||!character_read(p+0x11A,bones)||bones!=119
        ||!character_read(p+0x180,value.chain)||!value.chain||!character_read(value.chain+0x18,value.bin)||!value.bin
        ||!character_read(value.bin+0x29,bones)||bones!=119||!character_read(p+0x108,value.bones)||!value.bones
        ||!character_read(value.state+0x70,value.asset)||!value.asset
        ||!character_read(p+0x188,alternate)||alternate||!character_read(p+0x3B8,remap)||remap
        ||!character_read(p+0x3A0,effect)||!character_effect_supported(effect,flags)
        ||!character_read(value.state+0x5020,value.room))return false;
    // The preview body must be built from the current native Leon body BIN0.
    unsigned offset=0;
    if(!character_read(value.asset+0x10,offset)||!offset||value.bin!=value.asset+offset)return false;
    out=value;return true;
}
struct CharacterMenuRotation {
    CharacterIdentity owner{};
    std::uint64_t tick=0,frame=0;
} character_menu_rotation;
void character_menu_rotate(void* actor){
    std::uintptr_t preview=0;
    if(!main_base||!actor||!character_read(main_base+0x1272140,preview)
        ||preview!=reinterpret_cast<std::uintptr_t>(actor))return;
    CharacterIdentity identity;
    if(!character_menu_identity(actor,identity)){character_menu_rotation={};return;}
    const auto now=svcGetSystemTick(),frame=view_frame.load(std::memory_order_acquire);
    auto& rotation=character_menu_rotation;
    if(!(rotation.owner==identity)||!rotation.tick){rotation={identity,now,frame};return;}
    if(frame==rotation.frame)return;
    const auto previous=rotation.tick;rotation.tick=now;rotation.frame=frame;
    if(now<=previous||now-previous>re6::kTickHz/10)return;
    re6::Record input{};
    if(!telemetry_read_input(now,input))return;
    const float axis=re4::shape_axis(input.sticks[2]/32767.f,settings.right_deadzone,1,settings.stick_curve);
    float yaw=0;
    if(!axis||!std::isfinite(axis)||!character_read(identity.actor+0xB4,yaw)
        ||!std::isfinite(yaw)||std::fabs(yaw)>1e6f)return;
    // Native4BCC60 updates this exact preview through2B7C60. Its2B8700
    // evaluation rebuilds the actor matrix from B0/B4/B8 before skin/draw.
    // Keep the angle on release; native initialization resets a new preview.
    yaw=std::remainder(yaw+axis*2.09439510239f*float(now-previous)/float(re6::kTickHz),6.28318530718f);
    character_store(actor,0xB4,yaw);
}
HOOK_DEFINE_TRAMPOLINE(CharacterMenuDestroy) {
    static void Callback(void* menu){character_menu_invalidate();Orig(menu);}
};
bool character_texture_ready(unsigned pack){
    for(unsigned i=0;i<64;++i){
        const auto* entry=reinterpret_cast<const std::uint8_t*>(main_base+0x11829B0+i*16);
        if(character_value<std::uint32_t>(entry,8)==pack&&character_value<std::uintptr_t>(entry,0)&&entry[15]==1)return true;
    }
    return false;
}
std::uintptr_t character_texture_blob(unsigned pack){
    for(unsigned i=0;i<64;++i){
        const auto* entry=reinterpret_cast<const std::uint8_t*>(main_base+0x11829B0+i*16);
        if(character_value<unsigned>(entry,8)==pack&&entry[15]==1)return character_value<std::uintptr_t>(entry,0);
    }
    return 0;
}
bool character_texture_metadata(std::uintptr_t blob,unsigned pack,unsigned id,std::uintptr_t& metadata){
    unsigned count=0,actual=0;
    if(!character_read(blob,actual)||actual!=pack||!character_read(blob+4,count)||!count||count>256||id>=count
        ||!character_region(blob,8+count*8)||!character_read(blob+8+id*8,metadata)||!character_region(metadata,0x10))return false;
    unsigned size=0,owner=0;
    return character_read(metadata,size)&&size&&size<=128*1024*1024&&character_read(metadata+8,owner)&&owner==pack
        &&character_region(metadata+0x10,size);
}
bool character_appearance_reserve_textures(CharacterBank& b,std::uint32_t* diagnostic=nullptr){
    using S=re4::CharacterLoadingStage;
    if(diagnostic)*diagnostic=0;
    const auto reject=[&](S stage,unsigned observed=0){if(diagnostic)*diagnostic=re4::character_loading_diagnostic(stage,observed);return false;};
    if(!b.ready.texture_count||b.ready.texture_count>b.ready.texture_ids.size()
        ||!b.ready.piece_count||b.ready.piece_count>b.ready.pieces.size())return reject(S::UniqueCount);
    const auto pack=b.pack.load(std::memory_order_relaxed);
    const auto blob=character_texture_blob(pack);
    if(!blob||!character_native.texture_object)return reject(S::Blob);
    unsigned missing=0,free=0;
    for(unsigned i=0;i<b.ready.texture_count;++i){
        std::uintptr_t meta=0;if(!character_texture_metadata(blob,pack,b.ready.texture_ids[i],meta))return reject(S::Metadata,b.ready.texture_ids[i]);
        const auto index=character_value<unsigned>(reinterpret_cast<void*>(meta),4);
        if(index==0xffffffffu){++missing;continue;}
        if(index>=3000)return reject(S::DescriptorIndex,b.ready.texture_ids[i]);
        const auto* desc=reinterpret_cast<const void*>(main_base+0x11519A0+index*0x40);
        if(character_value<unsigned>(desc,0x2C)!=pack||character_value<unsigned>(desc,0x38)!=b.ready.texture_ids[i]
            ||character_value<unsigned>(desc,0x28)!=index)return reject(S::DescriptorOwner,b.ready.texture_ids[i]);
    }
    for(unsigned i=0;i<3000;++i){
        const auto* desc=reinterpret_cast<const void*>(main_base+0x11519A0+i*0x40);
        if(!character_value<unsigned>(desc,8)&&!character_value<unsigned>(desc,0x2C))++free;
    }
    if(missing>free)return reject(S::DescriptorCapacity,missing-free);
    // Native23D010/14 replaces ref+4's DDS ID with FF|descriptor index. Collect
    // ALL original keys before any creator can mutate a shared TPL reference.
    struct Reservation {
        const void* ref=nullptr;unsigned id=0;
        std::uint16_t width=0,height=0;
        unsigned format=0,wrap_s=0,wrap_t=0,mip=0;
    } reservations[256]{};
    unsigned n=0;
    for(unsigned p=0;p<b.ready.piece_count;++p){
        const auto tpl=b.ready.pieces[p].tpl;
        const auto count=character_value<unsigned>(tpl.data,4);
        if(!count||count>256)return reject(S::UniqueCount);
        const auto table=character_value<const std::uint8_t*>(tpl.data,8);
        for(unsigned i=0;i<count;++i){
            if(b.phase.load(std::memory_order_acquire)!=CharacterPhase::Reservations)return true;
            const auto* image=character_value<const std::uint8_t*>(table,i*16);
            const auto* ref=character_value<const std::uint8_t*>(image,8);
            const auto id=character_value<unsigned>(ref,4);
            if(character_value<unsigned>(ref,0)!=pack)return reject(S::Metadata,id);
            unsigned j=0;while(j<n&&reservations[j].id!=id)++j;if(j<n)continue;
            if(n==256)return reject(S::UniqueCount);
            reservations[n++]={ref,id,character_value<std::uint16_t>(image,2),character_value<std::uint16_t>(image,0),
                character_value<unsigned>(image,4),character_value<unsigned>(image,0x10),character_value<unsigned>(image,0x14),unsigned(image[0x25]!=image[0x26])};
        }
    }
    if(n!=b.ready.texture_count)return reject(S::UniqueCount,n);
    for(unsigned i=0;i<n;++i){
        unsigned j=0;while(j<b.ready.texture_count&&b.ready.texture_ids[j]!=reservations[i].id)++j;
        if(j==b.ready.texture_count)return reject(S::UniqueCount,n);
    }
    // Called only by the native scene producer. Reservations occur immediately;
    // GPU upload commands join the same queue as ordinary mesh texture binds.
    for(unsigned i=0;i<n;++i){
        if(b.phase.load(std::memory_order_acquire)!=CharacterPhase::Reservations)return true;
        const auto& r=reservations[i];alignas(16) std::uint8_t object[32]{};
        character_native.texture_object(object,r.ref,r.width,r.height,r.format,r.wrap_s,r.wrap_t,r.mip);
        if(!character_value<std::uintptr_t>(object,0))return reject(S::Creator,r.id);
    }
    return true;
}
void character_appearance_wait_load(CharacterBank* b);
CharacterBank* character_loading_bank=nullptr; // Graphics worker, scoped through the synchronous budget call.
void character_appearance_native_load_and_wait(void* ctx,unsigned file,unsigned options){
    auto* b=character_loading_bank;if(!b||!character_native.load_texture)return;
    b->load_result.store(1,std::memory_order_release);
    character_native.load_texture(ctx,file,options);
    character_appearance_wait_load(b);
}
void character_appearance_load_command(void* command){
    auto* b=character_value<CharacterBank*>(command,0x10);
    if(b!=&character_banks[0]&&b!=&character_banks[1])return;
    const auto token=character_value<std::uint64_t>(command,0x18);
    if(b->phase.load(std::memory_order_acquire)!=CharacterPhase::Probe||b->token.load(std::memory_order_acquire)!=token)return;
    auto* ctx=character_native.context?character_native.context(character_value<std::uint16_t>(command,0xA)):nullptr;
    b->load_diagnostic.store(0,std::memory_order_release);
    if(!ctx||!character_native.load_texture){b->load_diagnostic.store(re4::character_loading_diagnostic(re4::CharacterLoadingStage::Context),std::memory_order_release);b->load_result.store(2,std::memory_order_release);return;}
    bool retry=false;std::uint32_t diagnostic=0;
    character_loading_bank=b;
    const bool admitted=character_pack_budget_load(ctx,character_value<unsigned>(command,0x20),b->pack.load(std::memory_order_relaxed),
        character_appearance_native_load_and_wait,character_value<unsigned>(command,0x24),&retry,&diagnostic);
    character_loading_bank=nullptr;
    if(!admitted){
        // A surprising native handshake failure after load entry retains the
        // pin until its asynchronous lifetime is independently proven terminal.
        const auto load=b->load_result.load(std::memory_order_acquire);
        if(load!=1&&load!=3){b->load_diagnostic.store(diagnostic,std::memory_order_release);b->load_result.store(retry?5:2,std::memory_order_release);}
        return;
    }
    if(b->load_result.load(std::memory_order_acquire)==3)return;
    b->load_result.store(1,std::memory_order_release);
    character_appearance_wait_load(b); // Also joins a borrowed native pending pack.
}
void character_appearance_wait_load(CharacterBank* b){
    // The native loader starts an IO thread. Keep this worker command pending
    // until its exact copied request finishes; GPU completion alone says nothing
    // about a callback still writing the borrowed native texture blob.
    const auto pack=b->pack.load(std::memory_order_relaxed);
    std::uintptr_t entry=0,buffer=0;
    for(unsigned j=0;j<64;++j){
        const auto candidate=main_base+0x11829B0+j*16;
        if(character_value<unsigned>(reinterpret_cast<void*>(candidate),8)==pack){entry=candidate;buffer=character_value<std::uintptr_t>(reinterpret_cast<void*>(candidate),0);break;}
    }
    std::uintptr_t request=0;
    unsigned request_index=0;
    for(unsigned i=0;i<20;++i){
        const auto candidate=main_base+0x1184300+i*0x58;
        std::uintptr_t copied=0,association=0,destination=0,callback=0;
        if(!character_read(candidate,copied)||copied!=main_base+0x11849E0+i*0x60
            ||!character_read(copied+0x40,association)||association!=entry
            ||!character_read(candidate+8,destination)||destination!=buffer
            ||!character_read(candidate+0x18,callback)||(callback!=main_base+0x24E300&&callback!=main_base+0x24DCE0))continue;
        request=candidate;request_index=i;break;
    }
    if(!request)return; // Borrowed ready pack, or unknown IO retained until proof.
    std::uintptr_t wrapper=0,sdk=0;
    if(character_read(main_base+0x11852A0+request_index*8,wrapper)&&wrapper&&character_read(wrapper+0x18,sdk)&&sdk&&character_native.wait_thread){
        // The active game process retains the scheduler baton while blocked in
        // queue_load's two native flushes; main-producer GC280720 cannot run.
        character_native.wait_thread(reinterpret_cast<void*>(sdk));
    }
    unsigned active=1;
    if(character_read(request+0x50,active)&&active!=1&&!character_texture_ready(pack)){
        b->load_diagnostic.store(re4::character_loading_diagnostic(re4::CharacterLoadingStage::NativeIo),std::memory_order_release);
        b->load_result.store(3,std::memory_order_release);
    }
}
bool character_appearance_queue_load(CharacterBank& b,const re4::CharacterAssetRecipe& recipe){
    if(!character_native.flush)return false;
    auto* command=character_native.command?character_native.command(0x28):nullptr;if(!command)return false;
    character_store(command,0,&character_appearance_load_command);character_store(command,0x10,&b);
    character_store(command,0x18,b.token.load(std::memory_order_acquire));
    character_store(command,0x20,recipe.file_index);character_store(command,0x24,0u);
    character_native.flush();character_native.flush();return true;
}
re4::CharacterAppearanceError character_appearance_waiting(const CharacterBank* bank){
    using E=re4::CharacterAppearanceError;
    if(!bank)return E::WaitingRetirement;
    switch(bank->phase.load(std::memory_order_acquire)){
        case CharacterPhase::Reading:{
            const auto* recipe=re4::character_asset_recipe(bank->appearance);
            return re4::character_appearance_diagnostic(E::ReadingAsset,re4::character_loading_percent(bank->read,recipe?recipe->size:0));
        }
        case CharacterPhase::Probe:{
            const auto load=bank->load_result.load(std::memory_order_acquire);
            if(load==5)return E::WaitingPackBusy;
            if(!load)return E::WaitingPackCommand;
            if(!character_texture_ready(bank->pack.load(std::memory_order_relaxed)))return E::WaitingPackIO;
            return E::WaitingProbeFence;
        }
        case CharacterPhase::UploadProbe:return E::WaitingUploadFence;
        case CharacterPhase::Active:return bank->draw_wait.load(std::memory_order_acquire);
        case CharacterPhase::Reservations:case CharacterPhase::CancelReservations:return E::WaitingReservations;
        default:return E::WaitingRetirement;
    }
}
void character_appearance_update(void* actor){
    character_appearance_collect();
    auto desired=settings.character_appearance;
    if(!re4::valid_character_appearance(desired))desired=re4::CharacterAppearance::Leon;
    if(desired==re4::CharacterAppearance::Leon&&character_desired==desired){
        character_appearance_publish(desired,re4::CharacterAppearanceStatus::Native);return;
    }
    CharacterIdentity identity;re4::CharacterAppearanceError identity_wait=re4::CharacterAppearanceError::None;
    const bool allowed=character_identity(actor,identity,&identity_wait);
    if(desired!=character_desired){
        character_desired=desired;character_error=re4::CharacterAppearanceError::None;
        for(auto& b:character_banks){character_appearance_retire(b);b.fault.store(0,std::memory_order_release);}
    }
    for(auto& b:character_banks){
        const auto phase=b.phase.load(std::memory_order_acquire);
        if((phase==CharacterPhase::Active||phase==CharacterPhase::Probe||phase==CharacterPhase::UploadProbe)
            &&(!allowed||!(b.identity==identity)))character_appearance_retire(b);
        const auto fault=b.fault.load(std::memory_order_acquire);
        if(b.appearance==desired&&fault){character_error=static_cast<re4::CharacterAppearanceError>(fault);character_appearance_retire(b);}
    }
    if(desired==re4::CharacterAppearance::Leon){character_appearance_publish(desired,re4::CharacterAppearanceStatus::Native);return;}
    if(character_error!=re4::CharacterAppearanceError::None){
        character_appearance_publish(re4::CharacterAppearance::Leon,re4::CharacterAppearanceStatus::Failed);return;
    }
    CharacterBank* working=nullptr;
    for(auto& b:character_banks)if(b.phase.load(std::memory_order_acquire)!=CharacterPhase::Free
        &&b.phase.load(std::memory_order_relaxed)!=CharacterPhase::Retiring&&b.appearance==desired)working=&b;
    if(working&&working->phase.load(std::memory_order_acquire)==CharacterPhase::Active){
        const bool drawn=working->drawn.load(std::memory_order_acquire)==working->token.load(std::memory_order_acquire);
        character_appearance_publish(drawn?desired:re4::CharacterAppearance::Leon,
            drawn?re4::CharacterAppearanceStatus::Active:re4::CharacterAppearanceStatus::Pending,
            character_appearance_waiting(working));return;
    }
    const auto waiting=!allowed?identity_wait:!character_mount_ready.load(std::memory_order_acquire)
        ?re4::CharacterAppearanceError::WaitingMount:character_appearance_waiting(working);
    character_appearance_publish(re4::CharacterAppearance::Leon,re4::CharacterAppearanceStatus::Pending,waiting);
    if(!allowed||!character_mount_ready.load(std::memory_order_acquire))return;
    const auto* recipe=re4::character_asset_recipe(desired);if(!recipe)return;
    if(!working){
        for(auto& b:character_banks)if(b.phase.load(std::memory_order_acquire)==CharacterPhase::Free){working=&b;break;}
        if(!working)return;
        auto& b=*working;b.appearance=desired;b.read=0;b.ready={};b.map=re4::CharacterRetargetMap{};b.drawn.store(0);b.draw_wait.store(re4::CharacterAppearanceError::WaitingSkin);b.load_result.store(0);b.fault.store(0);b.load_diagnostic.store(0);
        char path[96]{};const auto n=std::strlen(character_mount),r=std::strlen(recipe->path);
        if(n+r+3>sizeof(path))return;
        std::memcpy(path,character_mount,n);std::memcpy(path+n,":/",2);std::memcpy(path+n+2,recipe->path,r+1);
        if(R_FAILED(nn::fs::OpenFile(&b.file,path,1))){character_error=re4::CharacterAppearanceError::FileOpen;return;}
        b.opened=true;long size=0;
        if(R_FAILED(nn::fs::GetFileSize(&size,b.file))||size<0||std::size_t(size)!=recipe->size||recipe->size>character_bank_size){
            nn::fs::CloseFile(b.file);b.opened=false;character_error=re4::CharacterAppearanceError::FileSize;return;
        }
        b.phase.store(CharacterPhase::Reading,std::memory_order_release);
    }
    auto& b=*working;
    if(b.phase.load(std::memory_order_acquire)==CharacterPhase::Reading){
        const auto chunk=std::min<std::size_t>(256*1024,recipe->size-b.read);
        if(R_FAILED(nn::fs::ReadFile(b.file,b.read,b.raw+b.read,chunk))){
            character_error=re4::CharacterAppearanceError::FileRead;character_appearance_retire(b);return;
        }
        b.read+=chunk;
        character_appearance_publish(re4::CharacterAppearance::Leon,re4::CharacterAppearanceStatus::Pending,
            re4::character_appearance_diagnostic(re4::CharacterAppearanceError::ReadingAsset,re4::character_loading_percent(b.read,recipe->size)));
        if(b.read<recipe->size)return;
        nn::fs::CloseFile(b.file);b.opened=false;
        if(!re4::character_asset_ready(desired,b.raw,b.read,b.ready)){
            character_error=re4::CharacterAppearanceError::AssetMismatch;character_appearance_retire(b);return;
        }
        if(!character_appearance_prepare(b,identity)){
            character_error=re4::CharacterAppearanceError::UnsupportedRig;character_appearance_retire(b);return;
        }
        CharacterIdentity after;
        if(!character_identity(actor,after)||!(after==identity)){
            character_error=re4::CharacterAppearanceError::RendererChanged;character_appearance_retire(b);return;
        }
        if(!character_native.command||!character_native.context||!character_native.finish||!character_native.load_texture){
            character_error=re4::CharacterAppearanceError::RendererUnavailable;character_appearance_retire(b);return;
        }
        b.pack.store(recipe->texture_pack,std::memory_order_relaxed);b.pinned.store(true,std::memory_order_release);
        b.token.store(character_serial.fetch_add(1,std::memory_order_acq_rel)+1,std::memory_order_release);
        b.phase.store(CharacterPhase::Probe,std::memory_order_release);
        if(!character_appearance_queue_load(b,*recipe)){character_error=re4::CharacterAppearanceError::RendererUnavailable;character_appearance_retire(b);return;}
    }
    const auto load=b.load_result.load(std::memory_order_acquire);
    if(load==5&&b.phase.load(std::memory_order_acquire)==CharacterPhase::Probe
        &&b.ack.load(std::memory_order_acquire)>=b.token.load(std::memory_order_acquire)){
        b.token.store(character_serial.fetch_add(1,std::memory_order_acq_rel)+1,std::memory_order_release);b.load_result.store(0,std::memory_order_release);
        if(!character_appearance_queue_load(b,*recipe)){character_error=re4::CharacterAppearanceError::RendererUnavailable;character_appearance_retire(b);}
        return;
    }
    if(load==2||load==3){
        const auto diagnostic=b.load_diagnostic.load(std::memory_order_acquire);
        character_error=static_cast<re4::CharacterAppearanceError>(diagnostic?diagnostic:re4::character_loading_diagnostic(
            load==3?re4::CharacterLoadingStage::NativeIo:re4::CharacterLoadingStage::Handshake));
        character_appearance_retire(b);return;
    }
    if(b.phase.load(std::memory_order_acquire)==CharacterPhase::Probe
        &&b.load_result.load(std::memory_order_acquire)==1
        &&b.ack.load(std::memory_order_acquire)>=b.token.load(std::memory_order_acquire)
        &&character_texture_ready(recipe->texture_pack)){
        auto expected=CharacterPhase::Probe;
        if(!b.phase.compare_exchange_strong(expected,CharacterPhase::Reservations,std::memory_order_acq_rel))return;
        std::uint32_t diagnostic=0;
        const bool success=character_appearance_reserve_textures(b,&diagnostic);
        // No retirement marker may be queued until all producer-side upload
        // calls finish, even when the native command allocator yields.
        b.token.store(character_serial.fetch_add(1,std::memory_order_acq_rel)+1,std::memory_order_release);
        expected=CharacterPhase::Reservations;
        if(success&&b.phase.compare_exchange_strong(expected,CharacterPhase::UploadProbe,std::memory_order_acq_rel))return;
        if(!success)character_error=static_cast<re4::CharacterAppearanceError>(diagnostic);
        b.token.store(character_serial.fetch_add(1,std::memory_order_acq_rel)+1,std::memory_order_release);
        b.phase.store(CharacterPhase::Retiring,std::memory_order_release);
    }
    if(b.phase.load(std::memory_order_acquire)==CharacterPhase::UploadProbe
        &&b.ack.load(std::memory_order_acquire)>=b.token.load(std::memory_order_acquire)){
        auto expected=CharacterPhase::UploadProbe;
        b.phase.compare_exchange_strong(expected,CharacterPhase::Active,std::memory_order_acq_rel);
    }
}

// Command header +8/+A is written by native24FB80 and must remain unchanged.
void character_appearance_marker(void* command){
    auto* ack=character_value<std::atomic<std::uint64_t>*>(command,0x18);
    bool owned=false,gpu_finish=false;
    for(auto& bank:character_banks){
        if(ack==&bank.ack){owned=true;gpu_finish=true;}
        for(auto& slot:bank.slot_ack)owned|=ack==&slot;
        for(auto& slot:bank.menu.slot_ack)owned|=ack==&slot;
    }
    if(!owned)return;
    if(!gpu_finish){
        // 244A50/239930/239A30 copy raw deformation arrays into native GPU
        // resources before returning. Ordered worker completion, rather than a
        // GPU stall, therefore admits REWRITING this slot's CPU arrays.
        character_appearance_ack(*ack,character_value<std::uint64_t>(command,0x10));return;
    }
    const auto context_id=character_value<std::uint16_t>(command,0xA);
    auto* software=character_native.context?character_native.context(context_id):nullptr;
    if(!software||!character_native.finish)return;
    const auto index=character_value<std::uint32_t>(software,0);
    if(index!=0)return; // Only the audited main graphics context is supported.
    std::uintptr_t gpu=0;if(!character_read(main_base+0x1180D40+0x140+index*8,gpu)||!gpu)return;
    character_native.finish(reinterpret_cast<void*>(gpu));
    character_appearance_ack(*ack,character_value<std::uint64_t>(command,0x10));
}
void* character_graphics_context(void* software){
    if(!software)return nullptr;
    const auto index=character_value<std::uint32_t>(software,0);
    std::uintptr_t gpu=0;
    return index==0&&character_read(main_base+0x1180D40+0x140,gpu)?reinterpret_cast<void*>(gpu):nullptr;
}
HOOK_DEFINE_TRAMPOLINE(CharacterFrameEnd) {
    static void Callback(void* framebuffer,unsigned present){
        Orig(framebuffer,present);
        if(character_teardown.load(std::memory_order_acquire)||!character_native.command)return;
        for(auto& b:character_banks){
            const auto phase=b.phase.load(std::memory_order_acquire);const auto token=b.token.load(std::memory_order_acquire);
            if(phase==CharacterPhase::Free||phase==CharacterPhase::Reading)continue;
            const auto queue=[](std::atomic<std::uint64_t>& ack,std::uint64_t value){
                if(!value||ack.load(std::memory_order_acquire)>=value)return;
                auto* command=character_native.command(0x20);if(!command)return;
                character_store(command,0,&character_appearance_marker);character_store(command,0x10,value);character_store(command,0x18,&ack);
            };
            if(phase==CharacterPhase::Reservations||phase==CharacterPhase::CancelReservations||phase==CharacterPhase::Invalidating)continue;
            if(phase==CharacterPhase::Probe||phase==CharacterPhase::UploadProbe||phase==CharacterPhase::Retiring)queue(b.ack,token);
            for(unsigned slot=0;slot<3;++slot){
                queue(b.slot_ack[slot],b.slot_token[slot].load(std::memory_order_acquire));
                queue(b.menu.slot_ack[slot],b.menu.slot_token[slot].load(std::memory_order_acquire));
            }
        }
    }
};
// Caller holds character_render_lock for all binding identity/prepared access.
CharacterBank* character_active(void* actor,void* chain,CharacterBinding** binding=nullptr,bool prepare=false){
    CharacterIdentity identity;const bool gameplay=character_identity(actor,identity);
    if(!gameplay&&!character_menu_identity(actor,identity))return nullptr;
    if(identity.chain!=reinterpret_cast<std::uintptr_t>(chain))return nullptr;
    for(auto& b:character_banks)if(b.phase.load(std::memory_order_acquire)==CharacterPhase::Active){
        if(gameplay&&b.identity==identity){if(binding)*binding=&b;return &b;}
        if(!gameplay){
            auto& target=b.menu;
            if(!(target.identity==identity)){
                if(!prepare||!character_binding_idle(target)||!target.deformed)continue;
                target.prepared=false;
                if(!character_appearance_prepare_binding(b,target,identity))continue;
            }
            if(target.prepared){if(binding)*binding=&target;return &b;}
        }
    }
    return nullptr;
}
// Publish only a confirmed rendered appearance. The immutable key is
// copied into atomics so camera callbacks can retain height during brief render
// lock contention without reading a concurrently rebuilt binding or map.
bool character_appearance_height(void* actor,float& out){
    CharacterIdentity identity;
    if(!character_identity(actor,identity))return false;
    if(!character_render_lock.test_and_set(std::memory_order_acquire)){
        bool result=false;float difference=0;
        for(auto& bank:character_banks){
            const auto token=bank.token.load(std::memory_order_acquire);
            if(bank.phase.load(std::memory_order_acquire)!=CharacterPhase::Active||!token||!bank.prepared
                ||!re4::character_asset_recipe(bank.appearance)||bank.fault.load(std::memory_order_acquire)
                ||bank.drawn.load(std::memory_order_acquire)!=token||!(bank.identity==identity)
                ||!re4::character_height_difference(bank.map,difference)){
                if(bank.height.actor.load(std::memory_order_relaxed)==identity.actor){
                    bank.height.appearance.store(0,std::memory_order_release);bank.height.token.store(0,std::memory_order_release);
                }
                continue;
            }
            auto& h=bank.height;unsigned bits=0;std::memcpy(&bits,&difference,sizeof(bits));
            h.actor.store(identity.actor,std::memory_order_relaxed);h.chain.store(identity.chain,std::memory_order_relaxed);
            h.bin.store(identity.bin,std::memory_order_relaxed);h.bones.store(identity.bones,std::memory_order_relaxed);
            h.asset.store(identity.asset,std::memory_order_relaxed);h.bindings.store(identity.bindings,std::memory_order_relaxed);
            h.state.store(identity.state,std::memory_order_relaxed);h.renderer.store(identity.renderer,std::memory_order_relaxed);
            h.room.store(identity.room,std::memory_order_relaxed);h.bits.store(bits,std::memory_order_relaxed);
            h.appearance.store(unsigned(bank.appearance),std::memory_order_relaxed);
            h.token.store(token,std::memory_order_release);
            result=bank.phase.load(std::memory_order_acquire)==CharacterPhase::Active
                &&bank.token.load(std::memory_order_acquire)==token;
            if(result)break;
        }
        character_render_lock.clear(std::memory_order_release);
        if(result)out=difference;
        return result;
    }
    for(auto& bank:character_banks){
        auto& h=bank.height;const auto token=h.token.load(std::memory_order_acquire);
        const auto appearance=h.appearance.load(std::memory_order_acquire);
        if(!token||!appearance||appearance>re4::character_asset_recipes.size()||bank.fault.load(std::memory_order_acquire)
            ||bank.phase.load(std::memory_order_acquire)!=CharacterPhase::Active
            ||bank.token.load(std::memory_order_acquire)!=token||bank.drawn.load(std::memory_order_acquire)!=token
            ||h.actor.load(std::memory_order_relaxed)!=identity.actor||h.chain.load(std::memory_order_relaxed)!=identity.chain
            ||h.bin.load(std::memory_order_relaxed)!=identity.bin||h.bones.load(std::memory_order_relaxed)!=identity.bones
            ||h.asset.load(std::memory_order_relaxed)!=identity.asset||h.bindings.load(std::memory_order_relaxed)!=identity.bindings
            ||h.state.load(std::memory_order_relaxed)!=identity.state||h.room.load(std::memory_order_relaxed)!=identity.room
            ||h.renderer.load(std::memory_order_relaxed)!=identity.renderer)continue;
        float value=0;const auto bits=h.bits.load(std::memory_order_relaxed);std::memcpy(&value,&bits,sizeof(value));
        if(!std::isfinite(value)||h.token.load(std::memory_order_acquire)!=token
            ||bank.token.load(std::memory_order_acquire)!=token||bank.phase.load(std::memory_order_acquire)!=CharacterPhase::Active)continue;
        out=value;return true;
    }
    return false;
}
// Qualify the exact case model globals independently of held-geometry admission.
// Thus an unsupported native held piece receives the same common world lift as
// the natural body, with no model/bone/yaw/descriptor writes.
bool character_case_view(void* actor,const float* native,re4::ViewMatrix3x4& out){
    if(!native)return false;
    std::uintptr_t body=0,pool=0;unsigned count=0,stride=0;
    if(!character_read(main_base+0x1272140,body)||!body
        ||!character_read(main_base+0xAFF4A8+8,pool)||pool!=body
        ||!character_read(main_base+0xAFF4A8+0x10,count)||!count||count>161
        ||!character_read(main_base+0xAFF4A8+0x14,stride)||stride!=0x3C8)return false;
    const auto p=reinterpret_cast<std::uintptr_t>(actor);bool member=p==body;
    for(unsigned h=0;!member&&h<2;++h){std::uintptr_t held=0,vptr=0;const unsigned index=h?3:1;
        member=count>index&&character_read(main_base+(h?0x1272158:0x1272148),held)&&held==p&&p==pool+index*stride
            &&character_read(p,vptr)&&vptr==main_base+0xAFF538;
    }
    CharacterIdentity identity;
    if(!member||!character_menu_identity(reinterpret_cast<void*>(body),identity))return false;
    unsigned slot=3;if(!character_read(identity.state+0x2C,slot)||slot>=3)return false;
    const auto frame=view_frame.load(std::memory_order_relaxed);
    for(auto& bank:character_banks)if(bank.phase.load(std::memory_order_acquire)==CharacterPhase::Active
        &&re4::character_asset_recipe(bank.appearance)&&!bank.fault.load(std::memory_order_acquire)){
        const auto& binding=bank.menu;const auto skin=binding.skinned[slot].load(std::memory_order_acquire);
        if(!binding.prepared||!(binding.identity==identity)||!binding.visual[slot].valid||!skin||!(skin==frame||skin+1==frame))continue;
        float difference=0;re4::ViewMatrix3x4 matrix;std::memcpy(&matrix,native,sizeof(matrix));
        return re4::character_height_difference(binding.map,difference)&&re4::character_height_case_view(matrix,difference,out);
    }
    return false;
}
// 8DF30 type0/mode0 ->322090(vptr B09A60)->3222C0. The initializer
// appends body-archive BIN9/TPL1 and stores precisely that part in face+58.
// Zero means no attached blade; a present but unproved part is omitted. This
// optional attachment must never withhold a separately validated donor body.
bool character_attached_source(const CharacterIdentity& identity,CharacterAttachedSource& out){
    out={};std::uintptr_t face=0,part=0,vptr=0;
    if(!character_read(identity.actor+0x950,face))return false;
    if(!face)return true;
    if((face&7)||!character_region(face,0x60)||!character_read(face+0x58,part))return false;
    if(!part)return true;
    if((part&7)||!character_region(part,0x1A8)||!character_read(identity.actor,vptr)||vptr!=main_base+0xB09A60)return false;
    std::uintptr_t archive=0;unsigned count=0,body=0,blade=0,tpl=0;
    if(!character_read(identity.state+0x70,archive)||!archive||(archive&7)||!character_region(archive,0x38)
        ||!character_read(archive,count)||count<10||count>256
        ||!character_read(archive+0x10,body)||!character_read(archive+0x34,blade)||!character_read(archive+0x14,tpl))return false;
    const auto offset_valid=[&](unsigned offset){return offset>=0x10+count*4&&offset<character_bank_size
        &&archive<=~std::uintptr_t(0)-offset;};
    if(!offset_valid(body)||!offset_valid(blade)||!offset_valid(tpl)||archive+body!=identity.bin)return false;
    CharacterAttachedSource value;value.part=part;value.archive=archive;
    std::uintptr_t part_vptr=0;std::uint8_t bones=0;std::uint16_t vertices=0,normals=0,animation=0;unsigned bin_flags=0;
    if(!character_read(part,part_vptr)||part_vptr!=main_base+0xB02088
        ||!character_read(part+0x18,value.bin)||value.bin!=archive+blade||!character_region(value.bin,0x98)
        ||!character_read(part+0x20,value.tpl)||value.tpl!=archive+tpl||!character_region(value.tpl,0x10)
        ||!character_read(value.bin+0x29,bones)||bones!=119
        ||!character_read(value.bin+0x58,vertices)||vertices!=character_native_blade_vertices
        ||!character_read(value.bin+0x5A,normals)||normals!=character_native_blade_vertices
        // Native2B4540 converts the file's offsets and sets bits20/21.
        // Require the fully prepared live BIN, never its raw-file flags.
        ||!character_read(value.bin+0x38,bin_flags)||bin_flags!=0xA0300300
        ||!character_read(part+8,value.flags)||(value.flags&~9u)||(value.flags&1u)!=1
        ||!character_read(part+0x148,animation)||animation
        ||!character_read(part+0xA7,value.alpha)||!character_read(part+0x74,value.matrix))return false;
    for(const auto& row:value.matrix.m)for(float component:row)if(!std::isfinite(component)||std::fabs(component)>1e9f)return false;
    std::uintptr_t seen[64]{},node=identity.chain;bool member=false;
    unsigned used=0;
    while(node){
        if(used==64||(node&7)||!character_region(node,0x1A8))return false;
        for(unsigned i=0;i<used;++i)if(seen[i]==node)return false;
        seen[used++]=node;member|=node==part;
        if(!character_read(node+0x28,node))return false;
    }
    if(!member)return false;
    out=value;return true;
}
bool character_matrix_product(const re4::ViewMatrix3x4& a,const re4::ViewMatrix3x4& b,re4::ViewMatrix3x4& out){
    re4::ViewMatrix3x4 value{};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){
        double n=c==3?a.m[r][3]:0;
        for(unsigned k=0;k<3;++k)n+=double(a.m[r][k])*b.m[k][c];
        value.m[r][c]=float(n);if(!std::isfinite(value.m[r][c]))return false;
    }
    out=value;return true;
}
re4::ViewMatrix3x4 character_translation(re4::ViewVec3 p){return {{{1,0,0,p.x},{0,1,0,p.y},{0,0,1,p.z}}};}
bool character_attached_matrix(const CharacterBank& bank,unsigned slot,const re4::ViewMatrix3x4& native,
        bool natural_geometry,re4::ViewMatrix3x4& out){
    if(slot>=3||!bank.visual[slot].valid)return false;
    const auto& frame=bank.visual[slot];const auto d=frame.shift[0];
    auto value=native;
    if(!character_matrix_product(character_translation(d),native,value))return false;
    if(natural_geometry){
        if(!frame.knife_valid||!character_matrix_product(value,character_translation({-d.x,-d.y,-d.z}),value)
            ||!character_matrix_product(value,frame.knife_half_turn,value))return false;
    }
    out=value;return true;
}
bool character_attached_capture(CharacterBank& bank,unsigned slot){
    bank.attached_valid[slot]=false;
    CharacterAttachedSource source;
    if(!character_attached_source(bank.identity,source))return false;
    if(source.part&&(source.flags&8u)){
        auto* destination=bank.attached_parts[slot];unsigned knife=re4::character_asset_piece_limit;
        if(bank.appearance==re4::CharacterAppearance::Krauser){
            if(bank.ready.piece_count>re4::character_asset_piece_limit)return false;
            for(unsigned i=0;i<bank.ready.piece_count;++i)if(bank.ready.pieces[i].role==re4::CharacterAssetRole::Knife){
                if(knife!=re4::character_asset_piece_limit)return false;
                knife=i;
            }
            if(knife==re4::character_asset_piece_limit)return false;
            std::memcpy(destination,bank.parts[knife],0x1A8);
            character_store(destination,8,(character_value<unsigned>(destination,8)&~8u)|(source.flags&8u));
            character_store(destination,0xA7,source.alpha);
            re4::ViewMatrix3x4 attached;
            if(!character_attached_matrix(bank,slot,source.matrix,true,attached))return false;
            character_store(destination,0x74,attached);
        }else {
            const auto used=bank.ready.deformed_slot_bytes;
            if((used&31)||used>bank.capacity||character_native_blade_bytes>bank.capacity-used)return false;
            std::uintptr_t positions=0,normals=0;
            constexpr std::size_t bytes=character_native_blade_vertices*12;
            if(!character_read(source.part+0xB0+slot*8,positions)||!character_read(source.part+0xC8+slot*8,normals)
                ||!character_region(positions,bytes)||!character_region(normals,bytes))return false;
            auto* target=bank.deformed[slot]+used;const auto start=reinterpret_cast<std::uintptr_t>(target);
            const auto separate=[&](std::uintptr_t p){return p<=~std::uintptr_t(0)-bytes&&(p+bytes<=start||p>=start+character_native_blade_bytes);};
            if(!separate(positions)||!separate(normals))return false;
            re4::ViewMatrix3x4 attached;
            if(!character_attached_matrix(bank,slot,source.matrix,false,attached))return false;
            std::memcpy(destination,reinterpret_cast<const void*>(source.part),0x1A8);
            character_store(destination,0x74,attached);
            std::memcpy(target,reinterpret_cast<const void*>(positions),bytes);
            std::memcpy(target+character_native_blade_array,reinterpret_cast<const void*>(normals),bytes);
            for(unsigned i=0;i<character_native_blade_vertices*3;++i){
                float p=0,n=0;std::memcpy(&p,target+i*4,4);std::memcpy(&n,target+character_native_blade_array+i*4,4);
                if(!std::isfinite(p)||!std::isfinite(n)||std::fabs(p)>1e9f||std::fabs(n)>1e9f)return false;
            }
            // Every index in this persistent descriptor names this snapshot's
            // own arrays, even if native frame-slot state changes later.
            for(unsigned i=0;i<3;++i){
                character_store(destination,0xB0+i*8,static_cast<void*>(target));
                character_store(destination,0xC8+i*8,static_cast<void*>(target+character_native_blade_array));
            }
        }
        character_store(destination,0x28,static_cast<void*>(nullptr));
    }
    bank.attached_source[slot]=source;bank.attached_valid[slot]=true;return true;
}
bool character_attached_link(CharacterBank& bank,unsigned slot,void*& tail){
    tail=nullptr;CharacterAttachedSource source;
    if(!bank.attached_valid[slot]||!character_attached_source(bank.identity,source)||!(source==bank.attached_source[slot]))return false;
    if(!source.part||!(source.flags&8u))return true;
    if(!bank.ready.piece_count||bank.ready.piece_count>re4::character_asset_piece_limit)return false;
    void* last=nullptr;
    for(unsigned i=0;i<bank.ready.piece_count;++i)if(bank.ready.pieces[i].role!=re4::CharacterAssetRole::Knife)last=bank.parts[i];
    if(!last||character_value<void*>(last,0x28))return false;
    character_store(last,0x28,static_cast<void*>(bank.attached_parts[slot]));tail=last;return true;
}
bool character_appearance_skin_binding(CharacterBank& bank,CharacterBinding& b,unsigned slot,std::uintptr_t state){
    if(!character_native.output||!character_native.positions||!character_native.normals
        ||!character_region(state+0x1CC,119*sizeof(re4::ViewMatrix3x4)))return false;
    auto* output=static_cast<re4::CharacterNativeMatrix*>(character_native.output());if(!output)return false;
    const auto* source=reinterpret_cast<const re4::ViewMatrix3x4*>(state+0x1CC);
    re4::CharacterRetargetPose pose;
    auto& frame=b.visual[slot];frame.valid=frame.knife_valid=false;
    for(auto& h:frame.held)h.captured=false;
    if(!re4::character_retarget_pose(b.map,source,119,pose))return false;
    for(unsigned i=0;i<3;++i)frame.shift[i]=pose.joint_shift[i==0?10:i==1?16:17];
    if(bank.appearance==re4::CharacterAppearance::Krauser&&bank.knife_orientation_valid)
        frame.knife_valid=re4::character_knife_half_turn(bank.knife_orientation,pose.core[10],frame.knife_half_turn);
    for(unsigned i=0;i<bank.ready.piece_count;++i){
        const auto& piece=bank.ready.pieces[i];auto* part=b.parts[i];
        const auto* weights=reinterpret_cast<const re4::CharacterWeight8*>(piece.weights.data);
        const auto blended=re4::character_retarget_blend(b.map,pose,weights,piece.weight_count,output,piece.weight_count);
        if(blended<0)return false;
        character_native.positions(character_value<void*>(part,0xB0+slot*8),character_value<const void*>(piece.bin.data,0x48),
            piece.position_weights.data,piece.vertex_count);
        character_native.normals(character_value<void*>(part,0xC8+slot*8),character_value<const void*>(piece.bin.data,0x50),
            piece.normal_weights.data,piece.normal_count);
    }
    frame.valid=true;return true;
}
void character_held_capture(CharacterBank&,CharacterBinding&,unsigned);
void character_held_skin(void*,void*,int);
HOOK_DEFINE_TRAMPOLINE(CharacterSkin) {
    static int Callback(void* actor,void* chain){
        const auto result=Orig(actor,chain);
        if(character_render_lock.test_and_set(std::memory_order_acquire))return result;
        CharacterBinding* target=nullptr;auto* b=character_active(actor,chain,&target,true);
        if(!b){character_held_skin(actor,chain,result);character_render_lock.clear(std::memory_order_release);return result;}
        using E=re4::CharacterAppearanceError;
        if(!result){b->draw_wait.store(E::WaitingNativeSkin,std::memory_order_release);character_render_lock.clear(std::memory_order_release);return result;}
        const auto state=modern_move_slot(0xB1AE08);unsigned slot=3;
        if(state)character_read(state+0x2C,slot);
        if(slot<3){
            target->skinned[slot].store(0,std::memory_order_release);
            if(target->slot_ack[slot].load(std::memory_order_acquire)>=target->slot_token[slot].load(std::memory_order_acquire)){
                if(character_appearance_skin_binding(*b,*target,slot,state)){
                    // These persistent descriptors are prepared only while this
                    // entire binding slot has no pending native/GPU consumer.
                    character_held_capture(*b,*target,slot);
                    // Attachment failure affects only the optional blade.
                    if(target==static_cast<CharacterBinding*>(b))character_attached_capture(*b,slot);
                    target->skinned[slot].store(view_frame.load(std::memory_order_relaxed),std::memory_order_release);
                    b->draw_wait.store(E::WaitingDraw,std::memory_order_release);
                }
                else {b->fault.store(unsigned(re4::CharacterAppearanceError::SkinningFailed),std::memory_order_release);character_appearance_request_retirement(*b);}
            }else b->draw_wait.store(E::WaitingSlotConsumer,std::memory_order_release);
        }else b->draw_wait.store(E::WaitingFrameSlot,std::memory_order_release);
        character_render_lock.clear(std::memory_order_release);return result;
    }
};
// BIN9 vertices are skinned in player-root space. Native4ED670 multiplies
// weaponRootWorld * part74; compensate only our part, never either native root.
bool character_knife_transform(const re4::ViewMatrix3x4& player,const re4::ViewMatrix3x4& weapon,re4::ViewMatrix3x4& out){
    re4::ViewVec3 origin;
    if(!re4::view_transform_point(player,{},origin)||!re4::view_inverse_point(weapon,origin,origin))return false;
    re4::ViewMatrix3x4 value{},linear=weapon;
    for(auto& row:linear.m)row[3]=0;
    for(unsigned column=0;column<3;++column){
        re4::ViewVec3 axis{player.m[0][column],player.m[1][column],player.m[2][column]};
        if(!re4::view_inverse_point(linear,axis,axis))return false;
        value.m[0][column]=axis.x;value.m[1][column]=axis.y;value.m[2][column]=axis.z;
    }
    value.m[0][3]=origin.x;value.m[1][3]=origin.y;value.m[2][3]=origin.z;out=value;return true;
}
// Existing draw hook also sees the independent native Leon knife actor. The
// factory344B30/constructor344FD0 and attachment344C60 define this exact owner.
// No face950/body-chain membership is involved, including when a gun is held.
CharacterBank* character_knife_owner(void* actor,void* chain,unsigned& piece,re4::ViewMatrix3x4& correction){
    const auto player=modern_move_slot(re4::profile::player_slot),p=reinterpret_cast<std::uintptr_t>(actor);
    std::uintptr_t vptr=0;std::uint8_t type=0;
    if(!player||p==player||!character_read(p,vptr)||vptr!=main_base+0xB0AAF8
        ||!character_read(p+0x118,type)||type!=0x24||!character_region(p,0x408))return nullptr;
    CharacterIdentity identity;
    if(!character_identity(reinterpret_cast<void*>(player),identity))return nullptr;
    CharacterBank* bank=nullptr;
    for(auto& candidate:character_banks)if(candidate.phase.load(std::memory_order_acquire)==CharacterPhase::Active
        &&candidate.appearance==re4::CharacterAppearance::Krauser&&candidate.identity==identity){bank=&candidate;break;}
    if(!bank||bank->ready.piece_count>re4::character_asset_piece_limit)return nullptr;
    piece=re4::character_asset_piece_limit;
    for(unsigned i=0;i<bank->ready.piece_count;++i)if(bank->ready.pieces[i].role==re4::CharacterAssetRole::Knife){
        if(piece!=re4::character_asset_piece_limit)return nullptr;
        piece=i;
    }
    if(piece==re4::character_asset_piece_limit)return nullptr;
    std::uintptr_t owner=0,held=0,parent=0,root=0,grip=identity.bones,alternate=0,remap=0,effect=0,native_chain=0;
    std::uint8_t bones=0;unsigned flags=0,player_flags=0;
    if(!character_read(player+0x938,owner)||!owner||!character_read(owner+0x38,held)||held!=p
        ||!character_read(p+0x3F8,parent)||parent!=player||!character_read(p+0x108,root)||!root
        ||!character_read(p+0x180,native_chain)||native_chain!=reinterpret_cast<std::uintptr_t>(chain)
        ||!native_chain||!character_region(native_chain,0x1A8)
        ||!character_read(p+0x188,alternate)||alternate||!character_read(p+0x3B8,remap)||remap
        ||!character_read(p+8,flags)||(flags&0x4000)||!character_read(p+0x3A0,effect)||!character_effect_supported(effect,flags)
        ||!character_read(p+0x11A,bones)||!bones||!character_read(player+8,player_flags))return nullptr;
    if(player_flags&0x2000){
        if(grip>~std::uintptr_t(0)-10*0x1F0)return nullptr;
        grip+=10*0x1F0;
    }else {
        std::uintptr_t seen[10]{};
        for(unsigned i=0;i<10;++i){
            if(!grip)return nullptr;
            for(unsigned j=0;j<i;++j)if(seen[j]==grip)return nullptr;
            seen[i]=grip;if(!character_read(grip+0x108,grip))return nullptr;
        }
        for(auto prior:seen)if(prior==grip)return nullptr;
    }
    if(!grip||!character_read(root+0x78,parent)||parent!=grip)return nullptr;
    // Initializer344C7C..94 builds exactly weaponarchive BIN2/TPL1. Reject
    // alternate or extended chains rather than hiding unrelated weapon pieces.
    std::uintptr_t archive=0,bin=0,tpl=0,next=0;unsigned bo=0,to=0;std::uint8_t bin_bones=0;
    if(!character_read(identity.state+0x58,archive)||!archive||!character_read(archive+0x18,bo)||!bo
        ||!character_read(archive+0x14,to)||!to||archive>~std::uintptr_t(0)-std::max(bo,to)
        ||!character_read(native_chain+0x18,bin)||bin!=archive+bo||!character_region(bin,0x98)
        ||!character_read(bin+0x29,bin_bones)||bin_bones!=bones
        ||!character_read(native_chain+0x20,tpl)||tpl!=archive+to||!character_region(tpl,0x10)
        ||!character_read(native_chain+0x28,next)||next)return nullptr;
    re4::ViewMatrix3x4 player_world{},weapon_world{};
    if(!character_read(identity.bones+0x18,player_world)||!character_read(root+0x18,weapon_world)
        ||!character_knife_transform(player_world,weapon_world,correction))return nullptr;
    return bank;
}
// Resolve the actual native base selection without calling native lookup or
// mutating linked/dense bones. IDs/count and every traversed span are bounded.
template<class Reader> bool character_model_bone_read(std::uintptr_t actor,unsigned id,std::uintptr_t& out,Reader& reader){
    unsigned flags=0;std::uint8_t count=0;std::uintptr_t node=0;
    if(!reader.value(actor+8,flags)||!reader.value(actor+0x11A,count)||id>=count
        ||!reader.value(actor+0x108,node)||!node)return false;
    if(flags&0x2000){
        if(node>~std::uintptr_t(0)-id*0x1F0)return false;
        node+=id*0x1F0;
    }else {
        std::uintptr_t seen[re4::character_bone_limit]{};
        for(unsigned i=0;i<id;++i){
            if(!node||!reader.region(node,0x110))return false;
            for(unsigned j=0;j<i;++j)if(seen[j]==node)return false;
            seen[i]=node;if(!reader.value(node+0x108,node))return false;
        }
        for(unsigned i=0;i<id;++i)if(seen[i]==node)return false;
    }
    if(!reader.region(node,0x78))return false;
    out=node;return true;
}
bool character_model_bone(std::uintptr_t actor,unsigned id,std::uintptr_t& out){CharacterMemoryReader reader;return character_model_bone_read(actor,id,out,reader);}
template<class Reader> bool character_held_parent_read(const CharacterBinding& binding,std::uintptr_t actor,bool menu,
        unsigned handle,std::uintptr_t& root,std::uintptr_t& parent,unsigned& index,Reader& reader){
    std::uintptr_t held=0,owner=0,chain=0,alternate=0,remap=0,effect=0,vptr=0;unsigned flags=0;
    if(menu){
        std::uintptr_t pool=0;unsigned count=0,stride=0;
        const unsigned model_index=handle?3:1;
        if(!reader.value(main_base+0xAFF4A8+8,pool)||pool!=binding.identity.actor
            ||!reader.value(main_base+0xAFF4A8+0x10,count)||count<=model_index||count>161
            ||!reader.value(main_base+0xAFF4A8+0x14,stride)||stride!=0x3C8
            ||actor!=pool+model_index*stride)return false;
        if(!reader.value(main_base+(handle?0x1272158:0x1272148),held)||held!=actor
            ||!reader.value(actor,vptr)||vptr!=main_base+0xAFF538)return false;
    }else {
        std::uintptr_t move=0,base=0;
        if(!reader.value(binding.identity.actor+0x938,owner)||!owner
            ||!reader.value(owner+0x38+handle*8,held)||held!=actor
            ||!reader.value(actor+0x3F8,owner)||owner!=binding.identity.actor
            ||!reader.value(actor,vptr)||!reader.value(vptr+0x28,move)||move!=main_base+0x2E8590
            // +58 is the weapon-specific fire routine (2E85E0), not a
            // drawing discriminator. All audited native cObjWep classes share
            // this base method at+98, including ordinary firearms.
            ||!reader.value(vptr+0x98,base)||base!=main_base+0x2E8D80)return false;
    }
    if(!reader.value(actor+0x108,root)||!root||!reader.value(root+0x78,parent)||!parent
        ||!reader.value(actor+0x180,chain)||!chain||!reader.value(actor+0x188,alternate)||alternate
        ||!reader.value(actor+0x3B8,remap)||remap||!reader.value(actor+8,flags)||(flags&0x4000)
        ||!reader.value(actor+0x3A0,effect)||!character_effect_supported_read(effect,flags,reader))return false;
    for(unsigned i=0;i<3;++i){
        std::uintptr_t core=0;
        if(character_model_bone_read(binding.identity.actor,i==0?10:i==1?16:17,core,reader)&&core==parent){index=i;return true;}
    }
    return false;
}
bool character_held_parent(const CharacterBinding& binding,std::uintptr_t actor,bool menu,
        unsigned handle,std::uintptr_t& root,std::uintptr_t& parent,unsigned& index){CharacterMemoryReader reader;return character_held_parent_read(binding,actor,menu,handle,root,parent,index,reader);}
template<class Reader> bool character_archive_member_read(std::uintptr_t archive,std::uintptr_t pointer,Reader& reader){
    unsigned count=0;
    if(!archive||!reader.value(archive,count)||!count||count>256||!reader.region(archive,0x10+count*4))return false;
    for(unsigned i=0;i<count;++i){
        unsigned offset=0;if(!reader.value(archive+0x10+i*4,offset))return false;
        if(offset>=0x10+count*4&&offset<character_bank_size&&archive<=~std::uintptr_t(0)-offset&&archive+offset==pointer)return true;
    }
    return false;
}
bool character_archive_member(std::uintptr_t archive,std::uintptr_t pointer){CharacterMemoryReader reader;return character_archive_member_read(archive,pointer,reader);}
template<class Reader> bool character_held_body_pair_read(std::uintptr_t archive,std::uintptr_t bin,std::uintptr_t tpl,Reader& reader){
    unsigned count=0;
    if(!reader.value(archive,count)||count>256||!character_archive_member_read(archive,bin,reader)
        ||!character_archive_member_read(archive,tpl,reader))return false;
    // Native3474A0 and inventory4CB51C..5B4 select these exact shared
    // grenade/egg resources from the player DAT, never from weapon motion DAT.
    for(unsigned family=0;family<2;++family){
        const unsigned bo=family?0x1F4:0x1A8;
        if(bo+4>0x10+count*4)continue;
        unsigned relative=0;if(!reader.value(archive+bo,relative)||archive>~std::uintptr_t(0)-relative||archive+relative!=bin)continue;
        for(unsigned t=0;t<3;++t){
            const unsigned to=family?0x1F8+t*4:0x1AC+t*8;
            if(to+4>0x10+count*4)continue;
            if(reader.value(archive+to,relative)&&archive<=~std::uintptr_t(0)-relative&&archive+relative==tpl)return true;
        }
    }
    return false;
}
template<class Reader> bool character_held_archive_read(const CharacterBinding& binding,std::uintptr_t actor,bool menu,std::uintptr_t& out,Reader& reader){
    std::uintptr_t chain=0,bin=0,tpl=0,archive=0;
    if(!reader.value(actor+0x180,chain)||!reader.region(chain,0x1A8)
        ||!reader.value(chain+0x18,bin)||!reader.value(chain+0x20,tpl))return false;
    if(menu){
        // Native4C73C4..73D8 /4C74B0..74C4 use the independently loaded
        // inventory preview DAT at GOTB26C98(1270F70)+258. It can differ from
        // the equipped weapon's state58 resource.
        if(reader.value(main_base+0x1270F70+0x258,archive)&&character_archive_member_read(archive,bin,reader)
            &&character_archive_member_read(archive,tpl,reader)){out=archive;return true;}
        if(!reader.value(binding.identity.state+0x70,archive)||!character_held_body_pair_read(archive,bin,tpl,reader))return false;
    }else {
        std::uintptr_t vptr=0;std::uint8_t type=0;
        if(!reader.value(actor,vptr))return false;
        if(vptr==main_base+0xB0AC48){
            if(!reader.value(actor+0x118,type)||type!=0x3C||!reader.value(binding.identity.state+0x70,archive)
                ||!character_held_body_pair_read(archive,bin,tpl,reader))return false;
        }else if(!reader.value(binding.identity.state+0x58,archive)||!character_archive_member_read(archive,bin,reader)
            ||!character_archive_member_read(archive,tpl,reader))return false;
    }
    out=archive;return true;
}
bool character_held_archive(const CharacterBinding& binding,std::uintptr_t actor,bool menu,std::uintptr_t& out){CharacterMemoryReader reader;return character_held_archive_read(binding,actor,menu,out,reader);}

bool character_emitter_current(CharacterBank& bank,re4::CharacterEmitterPose& out,
        std::uintptr_t (&pointers)[26],CharacterEmitterReader& reader){
    const auto token=bank.token.load(std::memory_order_acquire);
    re4::ModernMoveMemory memory;
    if(!reader.view(bank.identity.bones,0x140,memory))return false;
    re4::ViewMatrix3x4 world[26]{},inverse_bind[26]{};
    std::uintptr_t parents[26]{},next[26]{};
    unsigned flags=0;if(!reader.value(bank.identity.actor+8,flags))return false;
    auto node=bank.identity.bones;
    for(unsigned i=0;i<26;++i){
        if((node&7)||!memory.contains(node,0x140))return false;
        for(unsigned j=0;j<i;++j)if(pointers[j]==node)return false;
        pointers[i]=node;
        if(!memory.value(node+0x18,world[i])||!memory.value(node+0x110,inverse_bind[i])
            ||!memory.value(node+0x78,parents[i])||!memory.value(node+0x108,next[i]))return false;
        const auto expected=i?pointers[re4::character_core_parents[i]]:bank.identity.actor;
        if(parents[i]!=expected)return false;
        if(i<25){
            if(!next[i])return false;
            if((flags&0x2000)&&next[i]!=node+0x1F0)return false;
            node=next[i];
        }
    }
    auto& cache=bank.emitter;
    if(!cache.valid||cache.token!=token||!(cache.identity==bank.identity)
        ||std::memcmp(cache.bones,pointers,sizeof(cache.bones))
        ||std::memcmp(cache.world,world,sizeof(world))
        ||std::memcmp(cache.inverse_bind,inverse_bind,sizeof(inverse_bind))){
        re4::CharacterEmitterPose pose{};
        if(!re4::character_emitter_pose(bank.map,world,inverse_bind,pose)){cache.valid=false;return false;}
        cache.identity=bank.identity;cache.token=token;cache.pose=pose;
        std::memcpy(cache.bones,pointers,sizeof(cache.bones));
        std::memcpy(cache.world,world,sizeof(world));
        std::memcpy(cache.inverse_bind,inverse_bind,sizeof(inverse_bind));cache.valid=true;
    }
    out=cache.pose;return true;
}
bool character_emitter_owner(const CharacterBinding& binding,std::uintptr_t actor,unsigned handle,
        unsigned& parent_index,CharacterEmitterReader& reader){
    std::uintptr_t root=0,parent=0,archive=0;
    if(!character_held_parent_read(binding,actor,false,handle,root,parent,parent_index,reader)
        ||!character_held_archive_read(binding,actor,false,archive,reader))return false;
    // Admit a visual displacement only after this exact immutable geometry
    // owner has actually used an owned held draw. A new/unsupported native
    // weapon must retain native emitters until its visual path is confirmed.
    std::uintptr_t chain=0;if(!reader.value(actor+0x180,chain))return false;
    for(unsigned slot=0;slot<3;++slot){
        const auto& frame=binding.visual[slot];const auto& held=frame.held[handle];
        if(!frame.valid||!binding.skinned[slot].load(std::memory_order_acquire)
            ||!held.captured||!held.drawn||!held.count||held.count>character_held_piece_limit
            ||held.actor!=actor||held.chain!=chain||held.root!=root||held.parent!=parent||held.archive!=archive)continue;
        bool exact=true;
        for(unsigned i=0;i<held.count;++i){
            if(!reader.region(held.source_parts[i],0x1A8)){exact=false;break;}
            const auto* native=reinterpret_cast<const std::uint8_t*>(held.source_parts[i]);
            // Match held Draw's immutable descriptor identity. Only Skin's six
            // weighted output pointers are dynamic; matrix/material/links stay exact.
            if(held.weighted[i]){
                if(std::memcmp(held.original[i],native,0xB0)
                    ||std::memcmp(held.original[i]+0xE0,native+0xE0,0x1A8-0xE0)){exact=false;break;}
            }else if(std::memcmp(held.original[i],native,0x1A8)){exact=false;break;}
        }
        if(exact)return true;
    }
    return false;
}
bool character_emitter_record(std::uintptr_t matrix,std::uintptr_t actor,CharacterEmitterReader& reader){
    if(matrix<0x40||!character_native.effect_script)return false;
    const auto record=matrix-0x40;
    std::uintptr_t pool=0;unsigned count=0;
    if(!reader.value(main_base+0x113CC00,count)||!count
        ||!reader.value(main_base+0x113CC10,pool)||!pool||record<pool
        ||(record-pool)%0x108||(record-pool)/0x108>=count)return false;
    re4::ModernMoveMemory memory;
    if(!reader.view(record,0x108,memory))return false;
    std::uint8_t active=0,type=0,group=0,variant=0;std::uint16_t part=0;
    std::uintptr_t owner=0,script=0;unsigned generation=0,current_generation=0;
    if(!memory.value(record+0x18,active)||(active&3)!=1||!memory.value(record+0x19,type)||type!=0x10
        ||!memory.value(record+0x28,owner)||owner!=actor||!memory.value(record+0x30,generation)
        ||!reader.value(actor+0x110,current_generation)||generation!=current_generation
        ||!memory.value(record+0x38,part)||part!=0xFE||!memory.value(record+3,group)
        ||!memory.value(record+0x10,variant)||!memory.value(record+0x20,script)||!script)return false;
    unsigned variants=0;
    switch(group){
        case 0x38:case 0x3B:case 0x3D:case 0x48:variants=2;break;
        case 0x3E:case 0x3F:case 0x49:case 0x4C:variants=1;break;
        default:return false;
    }
    return variant<variants&&reinterpret_cast<std::uintptr_t>(character_native.effect_script(group,variant,1))==script;
}
bool character_emitter_input(re4::CharacterEmitterSite site,const re4::ViewMatrix3x4* native,
        re4::ViewMatrix3x4& out){
    using S=re4::CharacterEmitterSource;
    if(site.source==S::None||!native||character_teardown.load(std::memory_order_acquire)
        ||!character_renderer_ready.load(std::memory_order_acquire))return false;
    bool possible=false;
    for(const auto& bank:character_banks)
        if(bank.phase.load(std::memory_order_acquire)==CharacterPhase::Active
            &&bank.token.load(std::memory_order_acquire))possible=true;
    if(!possible||character_render_lock.test_and_set(std::memory_order_acquire))return false;
    bool result=false;
    CharacterEmitterReader reader;CharacterIdentity identity;
    if(character_identity_read(reinterpret_cast<void*>(modern_move_slot(re4::profile::player_slot)),identity,reader)){
        for(auto& bank:character_banks){
            const auto token=bank.token.load(std::memory_order_acquire);
            if(bank.phase.load(std::memory_order_acquire)!=CharacterPhase::Active||!token||!bank.prepared
                ||!re4::character_asset_recipe(bank.appearance)||bank.fault.load(std::memory_order_acquire)
                ||bank.drawn.load(std::memory_order_acquire)!=token||!(bank.identity==identity))continue;
            const auto matrix=reinterpret_cast<std::uintptr_t>(native);
            bool matched=false,ambiguous=false;unsigned selected_parent=0;
            std::uintptr_t handler=0;
            if(!reader.value(identity.actor+0x938,handler)||!handler)continue;
            for(unsigned handle=0;handle<2;++handle){
                std::uintptr_t actor=0;unsigned parent=0;
                if(!reader.value(handler+0x38+handle*8,actor)||!actor
                    ||!character_emitter_owner(bank,actor,handle,parent,reader))continue;
                bool match=false;
                if(site.source==S::Emitter)match=character_emitter_record(matrix,actor,reader);
                else if(site.source==S::BodyHand){
                    std::uintptr_t bone=0;match=character_model_bone_read(identity.actor,10,bone,reader)&&matrix==bone+0x18;
                    parent=0; // These native producers explicitly consume BODY10.
                }else {
                    const unsigned begin=site.source==S::Laser?0:2,end=site.source==S::Laser?2:5;
                    for(unsigned id=begin;id<end;++id){
                        std::uintptr_t bone=0;
                        if(character_model_bone_read(actor,id,bone,reader)&&matrix==bone+0x18){match=true;break;}
                    }
                }
                if(match){
                    if(matched&&selected_parent!=parent){ambiguous=true;break;}
                    selected_parent=parent;matched=true;
                }
            }
            if(!matched||ambiguous)continue;
            re4::CharacterEmitterPose pose{};std::uintptr_t pointers[26]{};
            re4::ViewMatrix3x4 source{},value{};
            if(character_emitter_current(bank,pose,pointers,reader)&&reader.value(matrix,source)
                &&re4::character_emitter_translate(source,pose.delta[selected_parent],value)
                &&bank.phase.load(std::memory_order_acquire)==CharacterPhase::Active
                &&bank.token.load(std::memory_order_acquire)==token&&!bank.fault.load(std::memory_order_acquire)
                &&character_renderer_generation.load(std::memory_order_acquire)==identity.renderer
                &&character_renderer_ready.load(std::memory_order_acquire)&&!character_teardown.load(std::memory_order_acquire)){
                out=value;result=true;
            }
            break;
        }
    }
    character_render_lock.clear(std::memory_order_release);return result;
}

HOOK_DEFINE_TRAMPOLINE(CharacterEmitterPoint){
    static void Callback(const re4::ViewMatrix3x4* matrix,const re4::ViewVec3* point,re4::ViewVec3* out){
#if defined(__aarch64__)
        const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_extract_return_addr(__builtin_return_address(0)));
#else
        const auto caller=character_emitter_caller;
#endif
        const auto site=re4::character_emitter_site(re4::CharacterEmitterMath::Point,caller-main_base);
        if(site.source==re4::CharacterEmitterSource::None){Orig(matrix,point,out);return;}
        re4::ViewMatrix3x4 corrected{};
        if(character_emitter_input(site,matrix,corrected))Orig(&corrected,point,out);
        else Orig(matrix,point,out);
    }
};
HOOK_DEFINE_TRAMPOLINE(CharacterEmitterProduct){
    static void Callback(const re4::ViewMatrix3x4* a,const re4::ViewMatrix3x4* b,re4::ViewMatrix3x4* out){
#if defined(__aarch64__)
        const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_extract_return_addr(__builtin_return_address(0)));
#else
        const auto caller=character_emitter_caller;
#endif
        const auto site=re4::character_emitter_site(re4::CharacterEmitterMath::Product,caller-main_base);
        if(site.source==re4::CharacterEmitterSource::None){Orig(a,b,out);return;}
        re4::ViewMatrix3x4 corrected{};
        if(character_emitter_input(site,site.operand?b:a,corrected))Orig(site.operand?a:&corrected,site.operand?&corrected:b,out);
        else Orig(a,b,out);
    }
};
HOOK_DEFINE_TRAMPOLINE(CharacterEmitterCopy){
    static void Callback(const re4::ViewMatrix3x4* matrix,re4::ViewMatrix3x4* out){
#if defined(__aarch64__)
        const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_extract_return_addr(__builtin_return_address(0)));
#else
        const auto caller=character_emitter_caller;
#endif
        const auto site=re4::character_emitter_site(re4::CharacterEmitterMath::Copy,caller-main_base);
        if(site.source==re4::CharacterEmitterSource::None){Orig(matrix,out);return;}
        re4::ViewMatrix3x4 corrected{};
        if(character_emitter_input(site,matrix,corrected))Orig(&corrected,out);
        else Orig(matrix,out);
    }
};

void character_held_capture(CharacterBank& bank,CharacterBinding& binding,unsigned slot){
    auto& frame=binding.visual[slot];const bool menu=&binding==&bank.menu;
    for(unsigned handle=0;handle<2;++handle){
        auto& snapshot=frame.held[handle];snapshot.captured=snapshot.drawn=false;snapshot.count=0;snapshot.geometry_used=0;
        std::uintptr_t actor=0,owner=0,root=0,parent=0,chain=0,archive=0;unsigned parent_index=0;
        if(menu){if(!character_read(main_base+(handle?0x1272158:0x1272148),actor))continue;}
        else if(!character_read(binding.identity.actor+0x938,owner)||!owner||!character_read(owner+0x38+handle*8,actor))continue;
        if(!actor||!character_region(actor,menu?0x3C8:0x408)
            ||!character_held_parent(binding,actor,menu,handle,root,parent,parent_index)
            ||!character_read(actor+0x180,chain)||!character_held_archive(binding,actor,menu,archive))continue;
        std::uintptr_t node=chain;bool valid=true;
        while(node){
            const unsigned i=snapshot.count;
            if(i==character_held_piece_limit||(node&7)||!character_region(node,0x1A8)){valid=false;break;}
            for(unsigned j=0;j<i;++j)if(snapshot.source_parts[j]==node)valid=false;
            if(!valid)break;
            std::uintptr_t bin=0,tpl=0,bind=0,positions=0,normals=0,part_vptr=0,base=0;
            unsigned flags=0,bin_flags=0;std::uint8_t records=0,weights=0,id=0;std::uint16_t extended=0,nv=0,nn=0,animation=0;std::uint8_t model_bones=0;
            if(!character_read(node,part_vptr)||part_vptr!=main_base+0xB02088
                ||!character_read(node+8,flags)||(flags&~9u)||!(flags&1u)
                ||!character_read(node+0x148,animation)||animation
                ||!character_read(node+0x18,bin)||!character_region(bin,0x98)||!character_archive_member(archive,bin)
                ||!character_read(node+0x20,tpl)||!character_region(tpl,0x10)||!character_archive_member(archive,tpl)
                ||!character_read(bin+0x38,bin_flags)||(bin_flags&0x300000)!=0x300000
                ||!character_read(bin+0x29,records)||!records||!character_read(actor+0x11A,model_bones)||records>model_bones
                ||!character_read(bin+0x28,weights)||!character_read(bin+0x42,extended)||extended>255
                ||!character_read(bin,bind)||!character_region(bind,std::size_t(records)*16)
                ||!character_read(bind,id)
                ||!character_read(bin+0x58,nv)||!nv||!character_read(bin+0x5A,nn)||!nn
                ||!character_read(bin+0x48,positions)||!character_region(positions,std::size_t(nv)*12)
                ||!character_read(bin+0x50,normals)||!character_region(normals,std::size_t(nn)*12)) {valid=false;break;}
            const bool weighted=records!=1||weights>1;
            if(weighted){
                // Standard native Weight8 + uint16 vertex/normal palette indices.
                // Other formats and morph paths retain their original drawing.
                if(!weights||extended!=weights||!binding.held_geometry){valid=false;break;}
                std::uintptr_t table=0,vi=0,ni=0;
                if(!character_read(bin+0x20,table)||!character_region(table,std::size_t(weights)*8)
                    ||!character_read(bin+0x70,vi)||!character_region(vi,std::size_t(nv)*2)
                    ||!character_read(bin+0x78,ni)||!character_region(ni,std::size_t(nn)*2)){valid=false;break;}
                for(unsigned bone=0;bone<records;++bone){
                    re4::CharacterBindBone b;std::uint8_t record[16];
                    std::memcpy(record,reinterpret_cast<void*>(bind+bone*16),16);
                    b.id=record[0];b.parent=record[1];std::memcpy(&b.position,record+4,12);
                    if(b.id>=model_bones||(b.parent!=255&&b.parent>=b.id)||!re4::view_finite(b.position)){valid=false;break;}
                }
                for(unsigned w=0;valid&&w<weights;++w){
                    re4::CharacterWeight8 value;if(!character_read(table+w*8,value)||value.links<1||value.links>3){valid=false;break;}
                    unsigned used=0;for(unsigned j=0;j<value.links;++j){
                        if(value.bone[j]>=model_bones){valid=false;break;}
                        if(j+1<value.links){if(value.percent[j]>100-used){valid=false;break;}used+=value.percent[j];}
                    }
                }
                for(unsigned array=0;valid&&array<2;++array)for(unsigned v=0;v<(array?nn:nv);++v){
                    std::uint16_t value=0;if(!character_read((array?ni:vi)+v*2,value)||value>=weights){valid=false;break;}
                }
                const auto bytes=[](unsigned n){return (std::size_t(n)*12+31)&~std::size_t(31);};
                const auto size=bytes(nv)+bytes(nn);
                if(!valid||size>character_held_geometry_capacity-snapshot.geometry_used){valid=false;break;}
                for(unsigned j=0;j<i;++j)if(character_value<std::uintptr_t>(snapshot.original[j],0x18)==bin)valid=false;
                if(!valid)break;
                snapshot.geometry_offset[i]=snapshot.geometry_used;snapshot.geometry_used+=size;
                base=root; // Native4ED670 for ordinary weighted pieces.
            }else if(!character_model_bone(actor,id,base)){valid=false;break;}
            snapshot.weighted[i]=weighted;snapshot.vertices[i]=nv;snapshot.normals[i]=nn;
            const auto matrix=character_value<re4::ViewMatrix3x4>(reinterpret_cast<void*>(node),0x74);
            for(const auto& row:matrix.m)for(float f:row)if(!std::isfinite(f)||std::fabs(f)>1e9f)valid=false;
            if(!valid)break;
            snapshot.source_parts[i]=node;snapshot.base_bones[i]=base;
            std::memcpy(snapshot.original[i],reinterpret_cast<void*>(node),0x1A8);
            std::memcpy(snapshot.parts[i],snapshot.original[i],0x1A8);
            ++snapshot.count;
            if(!character_read(node+0x28,node)){valid=false;break;}
        }
        if(!valid||!snapshot.count)continue;
        for(unsigned i=0;i<snapshot.count;++i)character_store(snapshot.parts[i],0x28,
            i+1<snapshot.count?static_cast<void*>(snapshot.parts[i+1]):nullptr);
        snapshot.actor=actor;snapshot.chain=chain;snapshot.root=root;snapshot.parent=parent;
        snapshot.archive=archive;snapshot.captured=true;
    }
}
// Record only the exact native held model's successful standard skin callback.
// Body and weapon can skin in either order; matching frame+slot is required at
// first draw. This metadata never exposes or rewrites a consumer-owned buffer.
void character_held_skin(void* actor,void* chain,int result){
    for(auto& bank:character_banks)if(bank.phase.load(std::memory_order_acquire)==CharacterPhase::Active)
        for(bool menu:{false,true}){
            std::uintptr_t body=menu?0:bank.identity.actor,body_chain=0;
            if(menu&&!character_read(main_base+0x1272140,body))continue;
            bool candidate=false;
            if(menu){
                for(unsigned h=0;h<2;++h){std::uintptr_t p=0;
                    if(character_read(main_base+(h?0x1272158:0x1272148),p)&&p==reinterpret_cast<std::uintptr_t>(actor))candidate=true;
                }
            }else {
                std::uintptr_t owner=0;
                if(body&&character_read(body+0x938,owner)&&owner)for(unsigned h=0;h<2;++h){std::uintptr_t p=0;
                    if(character_read(owner+0x38+h*8,p)&&p==reinterpret_cast<std::uintptr_t>(actor))candidate=true;
                }
            }
            if(!candidate)continue;
            if(!body||!character_read(body+0x180,body_chain))continue;
            CharacterBinding* binding=nullptr;
            if(character_active(reinterpret_cast<void*>(body),reinterpret_cast<void*>(body_chain),&binding,true)!=&bank||!binding)continue;
            for(unsigned handle=0;handle<2;++handle){
                std::uintptr_t root=0,parent=0,actual_chain=0,archive=0;unsigned index=0,slot=3;
                if(!character_region(reinterpret_cast<std::uintptr_t>(actor),menu?0x3C8:0x408)
                    ||!character_held_parent(*binding,reinterpret_cast<std::uintptr_t>(actor),menu,handle,root,parent,index)
                    ||!character_read(reinterpret_cast<std::uintptr_t>(actor)+0x180,actual_chain)||actual_chain!=reinterpret_cast<std::uintptr_t>(chain)
                    ||!character_read(binding->identity.state+0x2C,slot)||slot>=3
                    ||!character_held_archive(*binding,reinterpret_cast<std::uintptr_t>(actor),menu,archive))continue;
                binding->held_skin[slot][handle]={reinterpret_cast<std::uintptr_t>(actor),actual_chain,root,parent,archive,
                    view_frame.load(std::memory_order_relaxed),result>0};
            }
        }
}
CharacterBank* character_held_owner(void* actor,void* chain,CharacterBinding*& target,unsigned& handle){
    for(auto& bank:character_banks)if(bank.phase.load(std::memory_order_acquire)==CharacterPhase::Active)
        for(auto* binding:{static_cast<CharacterBinding*>(&bank),&bank.menu}){
            bool candidate=false;
            for(const auto& frame:binding->visual)for(const auto& h:frame.held)
                if(h.captured&&h.actor==reinterpret_cast<std::uintptr_t>(actor)&&h.chain==reinterpret_cast<std::uintptr_t>(chain))candidate=true;
            if(!candidate)continue;
            const bool menu=binding==&bank.menu;CharacterIdentity identity;
            const bool valid=menu?character_menu_identity(reinterpret_cast<void*>(binding->identity.actor),identity)
                :character_identity(reinterpret_cast<void*>(binding->identity.actor),identity);
            if(!valid||!(identity==binding->identity))continue;
            unsigned slot=3;if(!character_read(identity.state+0x2C,slot)||slot>=3||!binding->visual[slot].valid)continue;
            for(unsigned h=0;h<2;++h){const auto& held=binding->visual[slot].held[h];
                if(held.captured&&held.actor==reinterpret_cast<std::uintptr_t>(actor)&&held.chain==reinterpret_cast<std::uintptr_t>(chain)){
                    target=binding;handle=h;return &bank;
                }
            }
        }
    return nullptr;
}
bool character_held_draw(CharacterBank& bank,CharacterBinding& binding,unsigned slot,unsigned handle,void*& out){
    auto& snapshot=binding.visual[slot].held[handle];std::uintptr_t root=0,parent=0,archive=0,chain=0;unsigned index=0;
    if(!snapshot.captured||!character_held_parent(binding,snapshot.actor,&binding==&bank.menu,handle,root,parent,index)
        ||root!=snapshot.root||parent!=snapshot.parent||!character_read(snapshot.actor+0x180,chain)||chain!=snapshot.chain
        ||!character_held_archive(binding,snapshot.actor,&binding==&bank.menu,archive)||archive!=snapshot.archive)return false;
    re4::ViewMatrix3x4 player{};re4::ViewVec3 shift,origin;
    if(!character_read(binding.identity.bones+0x18,player)||!re4::view_transform_point(player,{},origin)
        ||!re4::view_transform_point(player,binding.visual[slot].shift[index],shift)
        ||!re4::view_store_point(double(shift.x)-origin.x,double(shift.y)-origin.y,double(shift.z)-origin.z,shift))return false;
    bool weighted=false;for(unsigned i=0;i<snapshot.count;++i)weighted|=snapshot.weighted[i];
    if(weighted){
        const auto& stamp=binding.held_skin[slot][handle];
        if(!stamp.valid||stamp.actor!=snapshot.actor||stamp.chain!=snapshot.chain||stamp.root!=root||stamp.parent!=parent
            ||stamp.archive!=archive||stamp.frame!=binding.skinned[slot].load(std::memory_order_acquire)||!binding.held_geometry)return false;
    }
    re4::ViewMatrix3x4 matrices[character_held_piece_limit]{},bases[character_held_piece_limit]{};
    std::uintptr_t outputs[character_held_piece_limit][2]{};
    for(unsigned i=0;i<snapshot.count;++i){
        if(!character_region(snapshot.source_parts[i],0x1A8))return false;
        const auto* native=reinterpret_cast<const std::uint8_t*>(snapshot.source_parts[i]);
        // Native4EC354/398 refreshes B0/C8 slot pointers after reservation.
        // Those six outputs are dynamic; all other descriptor bytes, including
        // geometry/material identity, flags, next and matrix, must stay exact.
        if(snapshot.weighted[i]){
            if(std::memcmp(snapshot.original[i],native,0xB0)
                ||std::memcmp(snapshot.original[i]+0xE0,native+0xE0,0x1A8-0xE0))return false;
            outputs[i][0]=character_value<std::uintptr_t>(native,0xB0+slot*8);
            outputs[i][1]=character_value<std::uintptr_t>(native,0xC8+slot*8);
        }else if(std::memcmp(snapshot.original[i],native,0x1A8))return false;
        if(!character_read(snapshot.base_bones[i]+0x18,bases[i]))return false;
        const auto& base=bases[i];re4::ViewVec3 local;
        if(!re4::view_store_point(double(base.m[0][3])+shift.x,double(base.m[1][3])+shift.y,double(base.m[2][3])+shift.z,local)
            ||!re4::view_inverse_point(base,local,local)
            ||!character_matrix_product(character_translation(local),character_value<re4::ViewMatrix3x4>(snapshot.original[i],0x74),matrices[i]))return false;
        if(snapshot.drawn&&std::memcmp(&matrices[i],snapshot.parts[i]+0x74,sizeof(matrices[i])))return false;
    }
    // Validate the complete native float3 output batch before any owned write.
    // Native4EC1A0 has completed4F24F0/4F2160 for this exact model/frame/slot.
    if(weighted&&!snapshot.drawn){
        const auto target=reinterpret_cast<std::uintptr_t>(binding.held_geometry[slot][handle]);
        for(unsigned i=0;i<snapshot.count;++i)if(snapshot.weighted[i])for(unsigned array=0;array<2;++array){
            const auto p=outputs[i][array];
            const std::size_t count=array?snapshot.normals[i]:snapshot.vertices[i],bytes=count*12;
            if(!p||p>~std::uintptr_t(0)-bytes||!character_region(p,bytes)
                ||!(p+bytes<=target||p>=target+character_held_geometry_capacity))return false;
            for(unsigned v=0;v<count;++v){
                const auto value=character_value<re4::ViewVec3>(reinterpret_cast<void*>(p),v*12);
                if(!re4::view_finite(value)||std::fabs(value.x)>1e9f||std::fabs(value.y)>1e9f||std::fabs(value.z)>1e9f)return false;
            }
        }
    }
    // Descriptor preparation was fenced by body Skin. Before its first held
    // consumer only these owned matrices may be finalized; subsequent draws
    // reuse byte-identical descriptors until that binding's GPU ack completes.
    if(!snapshot.drawn)for(unsigned i=0;i<snapshot.count;++i){
        if(snapshot.weighted[i]){
            auto* target=binding.held_geometry[slot][handle]+snapshot.geometry_offset[i];
            const auto normals=(std::size_t(snapshot.vertices[i])*12+31)&~std::size_t(31);
            std::memcpy(target,reinterpret_cast<void*>(outputs[i][0]),std::size_t(snapshot.vertices[i])*12);
            std::memcpy(target+normals,reinterpret_cast<void*>(outputs[i][1]),std::size_t(snapshot.normals[i])*12);
            for(unsigned frame=0;frame<3;++frame){
                character_store(snapshot.parts[i],0xB0+frame*8,static_cast<void*>(target));
                character_store(snapshot.parts[i],0xC8+frame*8,static_cast<void*>(target+normals));
            }
        }
        character_store(snapshot.parts[i],0x74,matrices[i]);
    }
    snapshot.drawn=true;out=snapshot.parts[0];return true;
}
HOOK_DEFINE_TRAMPOLINE(CharacterDraw) {
    static void Callback(void* actor,void* chain,const float* view,unsigned flags){
        if(character_render_lock.test_and_set(std::memory_order_acquire)){Orig(actor,chain,view,flags);return;}
        re4::ViewMatrix3x4 case_view;
        if(character_case_view(actor,view,case_view))view=&case_view.m[0][0];
        CharacterBinding* target=nullptr;auto* b=character_active(actor,chain,&target);
        unsigned knife=re4::character_asset_piece_limit,held=2;re4::ViewMatrix3x4 correction{};
        if(!b){b=character_knife_owner(actor,chain,knife,correction);if(b)target=b;}
        if(!b)b=character_held_owner(actor,chain,target,held);
        if(!b){character_render_lock.clear(std::memory_order_release);Orig(actor,chain,view,flags);return;}
        using E=re4::CharacterAppearanceError;
        unsigned slot=3;const auto state=modern_move_slot(0xB1AE08);if(state)character_read(state+0x2C,slot);
        const auto frame=view_frame.load(std::memory_order_relaxed);
        const auto skin=slot<3?target->skinned[slot].load(std::memory_order_acquire):0;
        const bool fresh=skin&&(skin==frame||skin+1==frame);
        if(fresh){
            void* owned=target->parts[0];bool valid=true;void* attached_tail=nullptr;
            if(held<2){valid=character_held_draw(*b,*target,slot,held,owned);
            }else if(knife<re4::character_asset_piece_limit){
                owned=target->parts[knife];unsigned visibility=0;std::uint8_t alpha=255;
                valid=target->visual[slot].valid&&target->visual[slot].knife_valid
                    &&character_read(reinterpret_cast<std::uintptr_t>(chain)+8,visibility)
                    &&character_read(reinterpret_cast<std::uintptr_t>(chain)+0xA7,alpha)
                    &&character_matrix_product(correction,target->visual[slot].knife_half_turn,correction);
                if(valid){
                    character_store(owned,8,(character_value<unsigned>(owned,8)&~8u)|(visibility&8u));
                    character_store(owned,0xA7,alpha);character_store(owned,0x74,correction);
                }
                // Native4ED670..6C4 consumes this owned matrix synchronously;
                // 250750/2507D0 copy48-byte matrices into command payloads.
            }else if(target==static_cast<CharacterBinding*>(b)){
                // Never replace a valid donor body with Leon for an omitted blade.
                character_attached_link(*b,slot,attached_tail);
            }
            if(valid){
                Orig(actor,owned,view,flags);
                if(attached_tail)character_store(attached_tail,0x28,static_cast<void*>(nullptr));
                target->slot_token[slot].store(character_serial.fetch_add(1,std::memory_order_acq_rel)+1,std::memory_order_release);
                if(held==2&&knife==re4::character_asset_piece_limit)b->drawn.store(b->token.load(std::memory_order_acquire),std::memory_order_release);
            }else if(held<2)Orig(actor,chain,view,flags);
            // An invalid optional Krauser flip omits only that blade.
        }else {
            b->draw_wait.store(slot>=3?E::WaitingFrameSlot:E::WaitingSkin,std::memory_order_release);
            Orig(actor,chain,view,flags);
        }
        character_render_lock.clear(std::memory_order_release);
    }
};

re4::CharacterTexturePins character_texture_pins(){
    re4::CharacterTexturePins pins;
    if(character_teardown.load(std::memory_order_acquire))return pins;
    for(auto& b:character_banks)if(b.pinned.load(std::memory_order_acquire))pins.ids[pins.count++]=b.pack.load(std::memory_order_relaxed);
    return pins;
}
re4::CharacterTextureTables character_texture_tables(){
    unsigned high=3001;character_read(main_base+0x1180D40+0x14,high);
    return {reinterpret_cast<void*>(main_base+0x11829B0),64,reinterpret_cast<void*>(main_base+0x11519A0),3000,high,
        reinterpret_cast<void*>(main_base+0x113E750),3000};
}
HOOK_DEFINE_TRAMPOLINE(CharacterTextureFree) {
    static void Callback(void* context,unsigned id){
        const auto pins=character_texture_pins();
        if(!pins.count){Orig(context,id);return;}
        const auto plan=re4::plan_character_textures({},pins,{re4::CharacterTextureOperation::Exact,id});
        if(plan.action==re4::CharacterTextureAction::Native)Orig(context,id);
    }
};
void character_texture_cleanup(void*){
    if(character_native.cleanup)character_native.cleanup();
    std::memset(reinterpret_cast<void*>(main_base+0x11837F0),0,0x960);
    const auto reset=modern_move_slot(0xB26EA0);if(reset)character_store(reinterpret_cast<void*>(reset),0,0u);
}
bool character_texture_policy(void* context,re4::CharacterTextureRequest request){
    const auto pins=character_texture_pins();if(!pins.count)return false;
    const auto tables=character_texture_tables();
    const auto plan=re4::plan_character_textures(tables,pins,request);
    if(plan.action==re4::CharacterTextureAction::Native)return false;
    if(plan.action==re4::CharacterTextureAction::RetireRenderer){
        // Unsupported native table state: hold protected resources, stop future
        // substitutions and let an actual frame-end fence retire them. Never
        // reinterpret a policy failure as permission to destroy live buffers.
        for(auto& b:character_banks){
            const auto phase=b.phase.load(std::memory_order_acquire);
            if(phase==CharacterPhase::Active||phase==CharacterPhase::Probe||phase==CharacterPhase::UploadProbe||phase==CharacterPhase::Reservations)
                b.fault.store(unsigned(re4::CharacterAppearanceError::RendererChanged),std::memory_order_release);
            character_appearance_request_retirement(b);
        }
        return true;
    }
    re4::CharacterTextureCallbacks callbacks{context,
        +[](void* ctx,unsigned id){CharacterTextureFree::Orig(ctx,id);},
        +[](void* ctx){auto* gpu=character_graphics_context(ctx);if(gpu&&character_native.reset_cache)character_native.reset_cache(gpu);},character_texture_cleanup};
    if(!re4::apply_character_texture_plan(plan,tables,callbacks))for(auto& b:character_banks){
        b.fault.store(unsigned(re4::CharacterAppearanceError::RendererChanged),std::memory_order_release);
        character_appearance_request_retirement(b);
    }
    return true;
}
HOOK_DEFINE_TRAMPOLINE(CharacterTextureGroup) {
    static void Callback(void* context,unsigned group,unsigned delay){
        if(!character_texture_policy(context,{re4::CharacterTextureOperation::Group,group,delay}))Orig(context,group,delay);
    }
};
HOOK_DEFINE_TRAMPOLINE(CharacterTextureMasked) {
    static void Callback(void* context,unsigned group,unsigned delay,unsigned mask){
        if(!character_texture_policy(context,{re4::CharacterTextureOperation::MaskedGroup,group,delay,mask}))Orig(context,group,delay,mask);
    }
};
HOOK_DEFINE_TRAMPOLINE(CharacterTextureSweep) {
    static void Callback(void* context){
        if(!character_texture_policy(context,{re4::CharacterTextureOperation::SelectiveSweep}))Orig(context);
    }
};
void character_appearance_join_loads(){
    if(!character_native.wait_thread)return;
    for(unsigned i=0;i<20;++i){
        const auto request=main_base+0x1184300+i*0x58;
        unsigned active=0;std::uintptr_t info=0,entry=0,buffer=0,callback=0,thread=0,sdk=0;
        if(!character_read(request+0x50,active)||active!=1||!character_read(request,info)
            ||info!=main_base+0x11849E0+i*0x60||!character_read(info+0x40,entry)
            ||!character_read(request+8,buffer)||!character_read(request+0x18,callback)
            ||(callback!=main_base+0x24E300&&callback!=main_base+0x24DCE0))continue;
        bool owned=false;
        for(auto& b:character_banks)if(b.pinned.load(std::memory_order_acquire)){
            const auto pack=b.pack.load(std::memory_order_relaxed);
            for(unsigned j=0;j<64;++j)if(entry==main_base+0x11829B0+j*16
                &&character_value<unsigned>(reinterpret_cast<void*>(entry),8)==pack
                &&character_value<std::uintptr_t>(reinterpret_cast<void*>(entry),0)==buffer)owned=true;
        }
        if(!owned||!character_read(main_base+0x11852A0+i*8,thread)||!thread||!character_read(thread+0x18,sdk)||!sdk)continue;
        // 6546A0 is nn::os::WaitThread, not the native wrapper destructor.
        // Its SDK object is collected only by the paused game producer280720.
        character_native.wait_thread(reinterpret_cast<void*>(sdk));
    }
}
HOOK_DEFINE_TRAMPOLINE(CharacterRendererDestroy) {
    static void Callback(){
        character_teardown.store(true,std::memory_order_release);
        character_renderer_ready.store(false,std::memory_order_release);
        character_renderer_generation.fetch_add(1,std::memory_order_acq_rel);
        for(auto& b:character_banks)character_appearance_retire(b);
        bool owns=false;for(auto& b:character_banks)owns|=b.pinned.load(std::memory_order_acquire);
        if(owns&&character_native.flush){
            // First flush swaps the existing producer list. The second flush's
            // FIRST wait (11841F8) acknowledges that whole list's execution.
            character_native.flush();character_native.flush();
        }
        if(owns)character_appearance_join_loads();
        Orig(); // Native GPUFinish, then worker stop, then full texture destruction.
        for(auto& b:character_banks)if(b.phase.load(std::memory_order_acquire)==CharacterPhase::Retiring){
            b.load_result.store(0,std::memory_order_release);
            character_appearance_ack(b.ack,b.token.load(std::memory_order_acquire));
            for(unsigned slot=0;slot<3;++slot){
                character_appearance_ack(b.slot_ack[slot],b.slot_token[slot].load(std::memory_order_acquire));
                character_appearance_ack(b.menu.slot_ack[slot],b.menu.slot_token[slot].load(std::memory_order_acquire));
            }
        }
        character_teardown.store(false,std::memory_order_release);
    }
};
HOOK_DEFINE_TRAMPOLINE(CharacterRendererInitialize) {
    static std::uintptr_t Callback(void* ctx,void* window,unsigned options,unsigned resolution,unsigned target){
        // Native first-call flag is set once at240B18; repeated calls return
        // before all resource clears. Full destruction remains a separate fence.
        const bool first=!(*reinterpret_cast<const std::uint8_t*>(main_base+0x11807CC)&1);
        if(first){character_renderer_ready.store(false,std::memory_order_release);character_renderer_generation.fetch_add(1,std::memory_order_acq_rel);}
        const auto result=Orig(ctx,window,options,resolution,target);
        if(first&&modern_move_slot(0x1180D40+0x140))character_renderer_ready.store(true,std::memory_order_release);
        return result;
    }
};
void character_appearance_install(){
    character_native.effect_script=reinterpret_cast<decltype(character_native.effect_script)>(main_base+0x8D470);
    character_appearance_reset();
    character_native.initialize=reinterpret_cast<decltype(character_native.initialize)>(main_base+0x2B66C0);
    character_native.attach=reinterpret_cast<decltype(character_native.attach)>(main_base+0x2B6740);
    character_native.output=reinterpret_cast<decltype(character_native.output)>(main_base+0x281D20);
    character_native.positions=reinterpret_cast<decltype(character_native.positions)>(main_base+0x4F24F0);
    character_native.normals=reinterpret_cast<decltype(character_native.normals)>(main_base+0x4F2160);
    character_native.texture_object=reinterpret_cast<decltype(character_native.texture_object)>(main_base+0x23CC10);
    character_native.load_texture=reinterpret_cast<decltype(character_native.load_texture)>(main_base+0x24DA70);
    character_native.command=reinterpret_cast<decltype(character_native.command)>(main_base+0x24FB80);
    character_native.context=reinterpret_cast<decltype(character_native.context)>(main_base+0x24FB20);
    character_native.finish=reinterpret_cast<decltype(character_native.finish)>(main_base+0x26FD20);
    character_native.flush=reinterpret_cast<decltype(character_native.flush)>(main_base+0x252390);
    character_native.wait_thread=reinterpret_cast<decltype(character_native.wait_thread)>(main_base+0x6546A0);
    character_native.reset_cache=reinterpret_cast<decltype(character_native.reset_cache)>(main_base+0x26F430);
    character_native.cleanup=reinterpret_cast<decltype(character_native.cleanup)>(main_base+0x2F4360);
    CharacterMenuDestroy::InstallAtOffset(0x4BD000);
    CharacterSkin::InstallAtOffset(0x4EC1A0);
    CharacterEmitterPoint::InstallAtOffset(0x279A30);
    CharacterEmitterProduct::InstallAtOffset(0x279460);
    CharacterEmitterCopy::InstallAtOffset(0x279710);
    CharacterDraw::InstallAtOffset(0x4ED250);
    CharacterFrameEnd::InstallAtOffset(0x24FE10);
    CharacterTextureFree::InstallAtOffset(0x23F4F0);
    CharacterTextureGroup::InstallAtOffset(0x24C730);
    CharacterTextureMasked::InstallAtOffset(0x24C8E0);
    CharacterTextureSweep::InstallAtOffset(0x222060);
    CharacterRendererDestroy::InstallAtOffset(0x24F240);
    CharacterRendererInitialize::InstallAtOffset(0x240AE0);
}

