// Money is changed only here, on the native input thread. This ledger is
// independent from settings/extra weapon upgrades and never edits native saves.
re4::MoneyStore money_store,money_candidate,money_disk;
std::array<std::uint8_t,re4::kMoneyStoreBytes> money_bytes{};
bool money_store_loaded=false,money_fresh_campaign=false;
unsigned money_campaign=0;
struct MoneyIdentity {unsigned slot=0;std::uint32_t crc=0;bool valid=false;} money_identity;
std::uint64_t money_next_poll=0;
constexpr std::uint32_t kMoneyOtherModes=0xC0805000;
constexpr const char* kMoneyPaths[]={"r4ctl:/config/re4-controls/money-bonus-a.bin","r4ctl:/config/re4-controls/money-bonus-b.bin"};

void money_reset(){
    money_store={};money_candidate={};money_disk={};money_store_loaded=false;
    money_fresh_campaign=false;money_campaign=0;money_identity={};money_next_poll=0;
    control.money_result=0;control.money_availability=0;control.money_ack=control.money_request;
}
bool money_store_follows(const re4::MoneyStore& older,const re4::MoneyStore& newer){
    if(newer.generation<=older.generation||newer.campaign_count<older.campaign_count||newer.alias_count<older.alias_count)return false;
    for(unsigned i=0;i<older.campaign_count;++i)if(older.claimed[i]&&!newer.claimed[i])return false;
    for(unsigned i=0;i<older.alias_count;++i){const auto& a=older.aliases[i];const auto& b=newer.aliases[i];
        if(a.slot!=b.slot||a.crc!=b.crc||a.campaign!=b.campaign)return false;
    }
    return true;
}
bool money_load_store(){
    if(money_store_loaded)return true;
    if(!file_mounted)return false;
    money_candidate={};bool found=false;
    for(const auto* path:kMoneyPaths){
        nn::fs::FileHandle file{};const auto opened=nn::fs::OpenFile(&file,path,1);
        if(opened==0x202)continue;
        if(R_FAILED(opened))return false;
        long size=0;
        const bool valid=R_SUCCEEDED(nn::fs::GetFileSize(&size,file))&&size==long(money_bytes.size())
            &&R_SUCCEEDED(nn::fs::ReadFile(file,0,money_bytes.data(),money_bytes.size()))
            &&re4::money_decode(money_bytes.data(),money_bytes.size(),money_disk)
            &&money_disk.generation&&money_disk.campaign_count;
        nn::fs::CloseFile(file);
        // An invalid present journal is not equivalent to an empty journal.
        if(!valid)return false;
        if(found){
            if(money_disk.generation>money_candidate.generation){
                if(!money_store_follows(money_candidate,money_disk))return false;
            }else{
                if(!money_store_follows(money_disk,money_candidate))return false;
                continue;
            }
        }
        money_candidate=money_disk;found=true;
    }
    money_store=money_candidate;money_store_loaded=true;return true;
}
bool money_write_store(){
    if(!file_mounted||!money_store_loaded||money_store.generation==UINT32_MAX)return false;
    money_candidate.generation=money_store.generation+1;
    re4::money_encode(money_candidate,money_bytes.data());
    const auto* path=kMoneyPaths[money_candidate.generation&1];
    nn::fs::CreateFile(path,money_bytes.size()); // Already-existing inactive journal is reused.
    nn::fs::FileHandle file{};if(R_FAILED(nn::fs::OpenFile(&file,path,2)))return false;
    const auto result=nn::fs::WriteFile(file,0,money_bytes.data(),money_bytes.size(),
        nn::fs::WriteOption::CreateOption(nn::fs::WriteOptionFlag_Flush));
    nn::fs::CloseFile(file);
    if(R_FAILED(result))return false;
    money_store=money_candidate;return true;
}
unsigned money_resolve_campaign(){
    if(money_campaign)return money_campaign;
    if(money_identity.valid)return re4::money_find_campaign(money_store,money_identity.slot,money_identity.crc);
    return 0;
}
unsigned money_prepare_campaign(){
    money_candidate=money_store;
    unsigned campaign=money_resolve_campaign();
    if(!campaign){
        // Only the first existing save may be enrolled without a title edge.
        // Unknown older/imported saves after that cannot mint fresh claims.
        if(!money_fresh_campaign&&(!money_identity.valid||money_store.campaign_count))return 0;
        campaign=re4::money_new_campaign(money_candidate);
        if(!campaign)return 0;
    }
    if(money_identity.valid&&!re4::money_bind_save(money_candidate,money_identity.slot,money_identity.crc,campaign))return 0;
    return campaign;
}
void money_discard_request(){
    control.money_result=0;control.money_availability=0;
    std::atomic_thread_fence(std::memory_order_release);control.money_ack=control.money_request;money_next_poll=0;
}
void money_on_load(unsigned slot,std::uint32_t crc){
    money_campaign=0;money_fresh_campaign=false;money_identity={slot,crc,slot<20};money_discard_request();
}
void money_on_save(unsigned slot,std::uint32_t crc){
    std::uintptr_t state=0;std::uint32_t flags=0;
    if(!main_base||!extended_read(main_base+0xB1AE08,state)||!state
        ||!extended_read(state+0x78,flags)||(flags&kMoneyOtherModes))return;
    if(!money_load_store())return;
    const auto campaign=money_prepare_campaign();if(!campaign)return;
    const auto before=money_candidate.alias_count;
    if(!re4::money_bind_save(money_candidate,slot,crc,campaign))return;
    if(money_candidate.campaign_count==money_store.campaign_count&&money_candidate.alias_count==money_store.alias_count&&before==money_candidate.alias_count)return;
    if(money_write_store()){money_campaign=campaign;money_fresh_campaign=false;}
}
bool money_wallet(std::uintptr_t& address,std::int32_t& wallet){
    std::int32_t mode=0;std::uintptr_t state=0,actor=0;std::uint32_t flags=0;
    std::uint8_t phase=0;re4::ModernMoveMemory memory;
    if(!main_base||!extended_read(main_base+0xAFF39C,mode)||mode!=2
        ||!extended_read(main_base+0xB1AE08,state)||!state||!extended_read(state+0x78,flags)||(flags&kMoneyOtherModes)
        ||!extended_read(main_base+re4::profile::player_slot,actor)||!actor
        ||!extended_read(actor+0x114,phase)||phase||!view_scene(reinterpret_cast<void*>(actor)))return false;
    address=state+0x501C;
    return modern_move_region(address,sizeof(wallet),true,memory)&&memory.value(address,wallet)&&wallet>=0&&wallet<=re4::kMoneyMaximum;
}
void money_poll(){
    const auto request=control.money_request;
    const bool pending=request!=control.money_ack;
    if(!pending&&control.frames<money_next_poll)return;
    money_next_poll=control.frames+60;
    std::uintptr_t address=0;std::int32_t wallet=0;
    auto availability=re4::MoneyAvailability::Unavailable;
    if(money_wallet(address,wallet)){
        if(!money_load_store())availability=re4::MoneyAvailability::StorageError;
        else{
            const auto campaign=money_resolve_campaign();
            if(re4::money_claimed(money_store,campaign))availability=re4::MoneyAvailability::Claimed;
            else if(money_prepare_campaign())availability=wallet<=re4::kMoneyMaximum-re4::kMoneyBonus?
                re4::MoneyAvailability::Ready:re4::MoneyAvailability::Capacity;
        }
    }
    auto result=re4::MoneyResult::Unavailable;
    if(pending){
        switch(availability){
            case re4::MoneyAvailability::Claimed:result=re4::MoneyResult::AlreadyClaimed;break;
            case re4::MoneyAvailability::Capacity:result=re4::MoneyResult::Capacity;break;
            case re4::MoneyAvailability::StorageError:result=re4::MoneyResult::StorageError;break;
            case re4::MoneyAvailability::Ready:{
                const auto campaign=money_prepare_campaign();
                if(campaign&&re4::money_claim(money_candidate,campaign)&&money_write_store()){
                    // Everything that can fail, including the flushed claim, is
                    // finished before this single main-thread wallet mutation.
                    wallet+=re4::kMoneyBonus;std::memcpy(reinterpret_cast<void*>(address),&wallet,sizeof(wallet));
                    money_campaign=campaign;money_fresh_campaign=false;availability=re4::MoneyAvailability::Claimed;result=re4::MoneyResult::Granted;
                }else{availability=re4::MoneyAvailability::StorageError;result=re4::MoneyResult::StorageError;}
                break;
            }
            default:break;
        }
        control.money_result=unsigned(result);
    }
    control.money_availability=unsigned(availability);
    if(pending){std::atomic_thread_fence(std::memory_order_release);control.money_ack=request;}
}
// Authenticated title-state7 commit, after the title fade. At this edge only,
// bit8 clear starts fresh story; restored tag3 starts a new cleared-save run.
// Checkpoint restore also sees tag3 and must never be used as a campaign edge.
HOOK_DEFINE_INLINE(MoneyCampaignStart){
    static void Callback(exl::hook::InlineFloatCtx* context){
        if(!context)return;
        std::uintptr_t state=0;std::uint32_t flags=0;std::uint8_t title_state=0,tag=0;
        if(!extended_read(main_base+0xB1AE08,state)||!state||state!=re4::saved_x(context,8)
            ||!extended_read(state+0x78,flags)||flags!=std::uint32_t(re4::saved_x(context,9))
            ||!extended_read(re4::saved_x(context,19),title_state)||title_state!=7)return;
        money_discard_request();
        if(flags&kMoneyOtherModes){money_campaign=0;money_identity={};money_fresh_campaign=false;return;}
        if(!(flags&0x100)||(extended_read(state+0x84F4,tag)&&tag==3)){
            money_campaign=0;money_identity={};money_fresh_campaign=true;
        }
    }
};
void money_install(){money_reset();MoneyCampaignStart::InstallAtOffset(0x4E8928);}
