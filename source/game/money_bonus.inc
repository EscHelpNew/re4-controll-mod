// One separate SD receipt, independent of native save identity and settings.
// Only the native input thread changes the wallet after a flushed receipt.
bool money_flag_known=false,money_received=false,money_legacy_checked=false;
std::uint64_t money_next_poll=0;
constexpr std::uint32_t kMoneyOtherModes=0xC0805000;
constexpr const char* kMoneyReceiptPath="r4ctl:/config/re4-controls/money-bonus.bin";
constexpr const char* kLegacyMoneyPaths[]={"r4ctl:/config/re4-controls/money-bonus-a.bin","r4ctl:/config/re4-controls/money-bonus-b.bin"};
std::array<std::uint8_t,re4::kLegacyMoneyStoreBytes> money_legacy_bytes{};
enum class MoneyReceiptWrite {Stored,Exists,Error};

void money_invalidate_request(){
    control.money_result=0;control.money_availability=0;
    std::atomic_thread_fence(std::memory_order_release);control.money_ack=control.money_request;money_next_poll=0;
}
void money_reset(){
    money_flag_known=money_received=money_legacy_checked=false;money_next_poll=0;money_invalidate_request();
}
MoneyReceiptWrite money_write_receipt(){
    if(!file_mounted)return MoneyReceiptWrite::Error;
    // CreateFile is exclusive: an existing receipt is never overwritten.
    if(R_FAILED(nn::fs::CreateFile(kMoneyReceiptPath,1))){
        nn::fs::FileHandle existing{};
        if(R_FAILED(nn::fs::OpenFile(&existing,kMoneyReceiptPath,1)))return MoneyReceiptWrite::Error;
        nn::fs::CloseFile(existing);money_received=money_flag_known=true;return MoneyReceiptWrite::Exists;
    }
    money_flag_known=false; // A partial receipt still blocks a later attempt by its presence.
    nn::fs::FileHandle file{};
    if(R_FAILED(nn::fs::OpenFile(&file,kMoneyReceiptPath,2)))return MoneyReceiptWrite::Error;
    const std::uint8_t received=1;
    const auto result=nn::fs::WriteFile(file,0,&received,sizeof(received),
        nn::fs::WriteOption::CreateOption(nn::fs::WriteOptionFlag_Flush));
    nn::fs::CloseFile(file);
    if(R_FAILED(result))return MoneyReceiptWrite::Error;
    money_received=money_flag_known=true;return MoneyReceiptWrite::Stored;
}
bool money_load_flag(bool force=false){
    if(money_flag_known&&(money_received||!force))return true;
    if(!file_mounted)return false;
    nn::fs::FileHandle file{};const auto opened=nn::fs::OpenFile(&file,kMoneyReceiptPath,1);
    if(R_SUCCEEDED(opened)){
        nn::fs::CloseFile(file);money_received=money_flag_known=true;return true;
    }
    if(opened!=0x202)return false;
    if(!money_legacy_checked){
        for(const auto* path:kLegacyMoneyPaths){
            nn::fs::FileHandle legacy{};const auto result=nn::fs::OpenFile(&legacy,path,1);
            if(result==0x202)continue;
            if(R_FAILED(result))return false;
            long size=0;
            const auto sized=nn::fs::GetFileSize(&size,legacy);
            const auto read=R_SUCCEEDED(sized)&&size==long(money_legacy_bytes.size())?
                nn::fs::ReadFile(legacy,0,money_legacy_bytes.data(),money_legacy_bytes.size()):0;
            nn::fs::CloseFile(legacy);
            if(R_FAILED(sized)||R_FAILED(read))return false;
            const bool claimed=size==long(money_legacy_bytes.size())
                &&re4::legacy_money_claimed(money_legacy_bytes.data(),money_legacy_bytes.size());
            if(claimed){
                money_write_receipt(); // Preserve an already-received bonus even if migration cannot flush.
                money_received=money_flag_known=true;return true;
            }
        }
        money_legacy_checked=true;
    }
    money_received=false;money_flag_known=true;return true;
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
        if(!money_load_flag(pending))availability=re4::MoneyAvailability::StorageError;
        else if(money_received)availability=re4::MoneyAvailability::Claimed;
        else availability=wallet<=re4::kMoneyMaximum-re4::kMoneyBonus?
            re4::MoneyAvailability::Ready:re4::MoneyAvailability::Capacity;
    }
    auto result=re4::MoneyResult::Unavailable;
    if(pending){
        switch(availability){
            case re4::MoneyAvailability::Claimed:result=re4::MoneyResult::AlreadyClaimed;break;
            case re4::MoneyAvailability::Capacity:result=re4::MoneyResult::Capacity;break;
            case re4::MoneyAvailability::StorageError:result=re4::MoneyResult::StorageError;break;
            case re4::MoneyAvailability::Ready:{
                const auto written=money_write_receipt();
                if(written==MoneyReceiptWrite::Stored){
                    wallet+=re4::kMoneyBonus;std::memcpy(reinterpret_cast<void*>(address),&wallet,sizeof(wallet));
                    availability=re4::MoneyAvailability::Claimed;result=re4::MoneyResult::Granted;
                }else if(written==MoneyReceiptWrite::Exists){
                    availability=re4::MoneyAvailability::Claimed;result=re4::MoneyResult::AlreadyClaimed;
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
