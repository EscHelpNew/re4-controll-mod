#include "game_link.hpp"
#include "re6/profile.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <limits>
namespace {
struct Re6Extents {u64 base,size;};
struct Re6Metadata {u64 pid,title;Re6Extents main,heap,alias,space;u8 build_id[32];};
static_assert(sizeof(Re6Metadata)==112);
bool re6_supported(const Re6Metadata& m){return m.title==re6::profile::title_id
    &&!std::memcmp(m.build_id,re6::profile::build_id.data(),32);}
Result re6_read(Service* s,u64 address,void* out,std::size_t size){
    const struct {u64 address,size;} range{address,size};
    return serviceDispatchIn(s,65102,range,.buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_Out},.buffers={{out,size}});
}
Result re6_write(Service* s,u64 address,const void* in,std::size_t size){
    const struct {u64 address,size;} range{address,size};
    return serviceDispatchIn(s,65103,range,.buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_In},.buffers={{in,size}});
}
bool re6_ready(const re6::SharedControl& c){return !std::memcmp(c.marker,re6::kSharedMarker,16)
    &&c.abi==re6::kSharedAbi&&c.status==1&&c.capabilities==re6::kCapabilityFiltering&&!c.reserved;}
}
void Re6GameLink::initialize(){
    info_ready_=R_SUCCEEDED(pminfoInitialize());
    if(R_FAILED(smGetService(&service_,"dmnt:cht")))message=tr("Debug service unavailable","Сервис отладки недоступен");
}
void Re6GameLink::close(){
    if(serviceIsActive(&service_))serviceClose(&service_);
    if(info_ready_)pminfoExit();
    info_ready_=false;process_=address_=0;
}
bool Re6GameLink::connect(){
    address_=process_=0;diagnostics={};details.clear();u64 pid=0,title=0;
    if(!info_ready_||R_FAILED(pmdmntGetApplicationProcessId(&pid))||R_FAILED(pminfoGetProgramId(&title,pid))
        ||title!=re6::profile::title_id){message=tr("Start Resident Evil 6","Запустите Resident Evil 6");return false;}
    if(!serviceIsActive(&service_)||R_FAILED(serviceDispatch(&service_,65003))){
        message=tr("Could not attach to RE6","Не удалось подключиться к RE6");return false;}
    Re6Metadata metadata{};
    if(R_FAILED(serviceDispatchOut(&service_,65002,metadata))||!re6_supported(metadata)||metadata.pid!=pid){
        message=tr("Unsupported RE6 build","Эта версия RE6 не поддерживается");return false;}
    std::array<MemoryInfo,32> maps{};
    std::array<std::uint8_t,0x1000+sizeof(re6::SharedControl)> buffer{};
    std::size_t budget=8*1024*1024;bool pending=false;
    for(u64 offset=0;offset<4096&&budget;){
        u64 count=0;
        if(R_FAILED(serviceDispatchInOut(&service_,65101,offset,count,
            .buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_Out},.buffers={{maps.data(),sizeof(maps)}}))
            ||!count||count>maps.size())break;
        for(u64 i=0;i<count&&budget;++i){
            const auto& m=maps[i];
            if((m.type&0xFF)!=MemType_CodeMutable||m.perm!=Perm_Rw
                ||m.size>std::numeric_limits<u64>::max()-m.addr)continue;
            const auto limit=std::min<u64>(m.size,2*1024*1024);
            for(u64 page=0;page<limit&&budget;page+=0x1000){
                const auto bytes=std::min<std::size_t>({buffer.size(),std::size_t(m.size-page),budget});
                budget-=bytes;
                if(bytes<sizeof(re6::SharedControl)||R_FAILED(re6_read(&service_,m.addr+page,buffer.data(),bytes)))break;
                for(std::size_t j=0;j+sizeof(re6::SharedControl)<=bytes;j+=8){
                    if(std::memcmp(buffer.data()+j,re6::kSharedMarker,16))continue;
                    re6::SharedControl c{};std::memcpy(&c,buffer.data()+j,sizeof(c));
                    if(c.abi==re6::kSharedAbi&&c.status==0&&c.capabilities==re6::kCapabilityFiltering)pending=true;
                    if(!re6_ready(c))continue;
                    re6::Settings applied;
                    if(!re6::decode_settings(c.settings,sizeof(c.settings),applied))continue;
                    address_=m.addr+page+j;process_=pid;diagnostics=c;
                    char text[448];std::snprintf(text,sizeof(text),
                        "RE6 abi=%u sequence=%u applied=%u source=%u frames=%llu\nfilter=%u samplers=%u overrides=%u AA_requested=%u",
                        c.abi,c.sequence,c.applied_sequence,c.settings_source,(unsigned long long)c.frames,
                        c.graphics_filter_latched,c.sampler_calls,c.sampler_overrides,unsigned(applied.anti_aliasing));
                    details=text;message=tr("Connected to RE6 module","Подключено к модулю RE6");return true;
                }
            }
        }
        offset+=count;
    }
    message=pending?tr("Settings initializing. Try again","Загрузка настроек. Попробуйте ещё раз"):
        tr("Mod not found. Install and restart RE6","Мод не найден. Установите и перезапустите RE6");return false;
}
bool Re6GameLink::apply(const re6::Settings& settings){
    if(!re6::valid_settings(settings))return false;
    if(!address_&&!connect())return false;
    Re6Metadata metadata{};
    if(R_FAILED(serviceDispatchOut(&service_,65002,metadata))||!re6_supported(metadata)||metadata.pid!=process_){
        address_=process_=0;message=tr("Game restarted. Reconnect","Игра перезапущена. Подключитесь заново");return false;}
    re6::SharedControl c{};
    if(R_FAILED(re6_read(&service_,address_,&c,sizeof(c)))||!re6_ready(c)){
        address_=process_=0;message=tr("Connection lost","Связь потеряна");return false;}
    if(settings.texture_filtering!=re6::TextureFilter::Native&&!(c.capabilities&re6::kCapabilityFiltering)){
        message=tr("Update the RE6 module","Обновите модуль RE6");return false;}
    std::uint8_t bytes[re6::kSettingsBytes];
    if(!re6::encode_settings(settings,bytes,sizeof(bytes)))return false;
    const u32 odd=(c.sequence+1)|1u,even=odd+1;
    const u64 sequence=address_+offsetof(re6::SharedControl,sequence);
    const bool sent=R_SUCCEEDED(re6_write(&service_,sequence,&odd,sizeof(odd)))
        &&R_SUCCEEDED(re6_write(&service_,address_+offsetof(re6::SharedControl,settings),bytes,sizeof(bytes)))
        &&R_SUCCEEDED(re6_write(&service_,sequence,&even,sizeof(even)));
    message=sent?tr("Settings sent to RE6","Настройки отправлены в RE6"):
        tr("Could not send settings","Не удалось отправить настройки");
    return sent;
}
