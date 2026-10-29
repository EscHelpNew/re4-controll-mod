#include "re4/home_launcher.hpp"
#include <switch.h>
#include <cstdio>
#include <cstring>

namespace re4 {
namespace {
bool verify(const std::uint8_t* data,std::size_t bytes,const std::uint8_t* expected){
    std::uint8_t hash[32];sha256CalculateHash(hash,data,bytes);
    return !std::memcmp(hash,expected,32);
}
class Backend final:public HomeBackend {
    bool ns_=false,ncm_=false,storage_=false,database_=false,had_previous_=false;
    NcmStorageId location_=NcmStorageId_SdCard;
    NcmContentStorage storage{};
    NcmContentMetaDatabase database{};
    NcmContentMetaKey key{};
    Service manager{};
    std::vector<std::uint8_t> previous_;
    std::string& detail_;
    bool check(Result rc,const char* step){
        if(R_SUCCEEDED(rc))return true;
        char text[100];std::snprintf(text,sizeof(text),"HOME %s: %08X",step,rc);detail_=text;return false;
    }
    bool read_database(std::vector<std::uint8_t>& out,bool& exists){
        if(!check(ncmContentMetaDatabaseHas(&database,&exists,&key),"metadata lookup"))return false;
        out.clear();if(!exists)return true;
        u64 size=0,actual=0;
        if(!check(ncmContentMetaDatabaseGetSize(&database,&size,&key),"metadata size") || size>4096)return false;
        out.resize(size);
        return check(ncmContentMetaDatabaseGet(&database,&key,&actual,out.data(),out.size()),"metadata read") && actual==size;
    }
    NcmContentId id(const HomeContent& content){NcmContentId result;std::memcpy(result.c,content.id.data(),16);return result;}
    bool content_exists(const HomeContent& content,bool& exists){
        auto content_id=id(content);
        if(!check(ncmContentStorageHas(&storage,&exists,&content_id),"content lookup"))return false;
        if(!exists)return true;
        s64 size=0;
        if(!check(ncmContentStorageGetSizeFromContentId(&storage,&size,&content_id),"content size")
            || size!=s64(content.data.size()))return false;
        std::vector<std::uint8_t> bytes(content.data.size());
        return check(ncmContentStorageReadContentIdFile(&storage,bytes.data(),bytes.size(),&content_id,0),"content read")
            && verify(bytes.data(),bytes.size(),content.hash.data());
    }
public:
    explicit Backend(std::string& detail):detail_(detail){}
    ~Backend(){
        if(database_)ncmContentMetaDatabaseClose(&database);
        if(storage_)ncmContentStorageClose(&storage);
        if(serviceIsActive(&manager))serviceClose(&manager);
        if(ncm_)ncmExit();if(ns_)nsExit();
    }
    bool begin()override{
        const auto type=appletGetAppletType();
        if(type!=AppletType_Application && type!=AppletType_SystemApplication){
            detail_="HOME shortcut requires application mode";return false;
        }
        if(!check(nsInitialize(),"service"))return false;ns_=true;
        if(!check(nsGetApplicationManagerInterface(&manager),"application manager"))return false;
        bool installed=false;
        if(!check(nsIsAnyApplicationEntityInstalled(kOwnLauncherId,&installed),"installed lookup"))return false;
        if(installed){
            bool found=false;
            NsApplicationContentMetaStatus list[8]{};s32 count=0;
            if(!check(nsListApplicationContentMetaStatus(kOwnLauncherId,0,list,8,&count),"installed storage") || count<1 || count>8)return false;
            for(s32 i=0;i<count;++i){
                const auto& entry=list[i];
                if(entry.application_id!=kOwnLauncherId || entry.meta_type!=NcmContentMetaType_Application)continue;
                if(entry.version!=0 || (entry.storageID!=NcmStorageId_SdCard && entry.storageID!=NcmStorageId_BuiltInUser)){
                    detail_="HOME launcher has unexpected installed metadata";return false;
                }
                location_=static_cast<NcmStorageId>(entry.storageID);found=true;break;
            }
            if(!found){detail_="HOME application record has no own base content";return false;}
        }
        if(!check(ncmInitialize(),"content manager"))return false;ncm_=true;
        if(!check(ncmOpenContentStorage(&storage,location_),"storage open"))return false;storage_=true;
        if(!check(ncmOpenContentMetaDatabase(&database,location_),"database open"))return false;database_=true;
        return true;
    }
    int current(const HomeBundle& bundle)override{
        key={bundle.title,bundle.version,NcmContentMetaType_Application,NcmContentInstallType_Full,{}};
        std::vector<std::uint8_t> old;bool exists=false;
        if(!read_database(old,exists))return -1;
        if(!exists || old!=bundle.database)return 0;
        for(const auto& content:bundle.contents){bool has=false;if(!content_exists(content,has))return -1;if(!has)return 0;}
        return 1;
    }
    bool put(const HomeContent& content)override{
        bool exists=false;if(!content_exists(content,exists))return false;if(exists)return true;
        auto content_id=id(content);NcmPlaceHolderId placeholder{};
        if(!check(ncmContentStorageGeneratePlaceHolderId(&storage,&placeholder),"placeholder id"))return false;
        if(!check(ncmContentStorageCreatePlaceHolder(&storage,&content_id,&placeholder,content.data.size()),"placeholder create"))return false;
        bool ok=check(ncmContentStorageWritePlaceHolder(&storage,&placeholder,0,content.data.data(),content.data.size()),"placeholder write")
            && check(ncmContentStorageFlushPlaceHolder(&storage),"placeholder flush")
            && check(ncmContentStorageRegister(&storage,&content_id,&placeholder),"content register");
        if(!ok)ncmContentStorageDeletePlaceHolder(&storage,&placeholder);
        return ok;
    }
    bool snapshot(const HomeBundle&)override{return read_database(previous_,had_previous_);}
    bool publish(const HomeBundle& bundle)override{
        return check(ncmContentMetaDatabaseSet(&database,&key,bundle.database.data(),bundle.database.size()),"metadata set")
            && check(ncmContentMetaDatabaseCommit(&database),"metadata commit");
    }
    bool register_record(const HomeBundle&)override{
        struct Record {NcmContentMetaKey key;u8 storage;u8 padding[7];};
        static_assert(sizeof(Record)==24);
        const Record record{key,u8(location_),{}};
        const struct {u8 event;u8 padding[7];u64 title;} in{3,{},kOwnLauncherId};
        return check(serviceDispatchIn(&manager,16,in,
            .buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_In},.buffers={{&record,sizeof(record)}}),"record update");
    }
    bool refresh()override{return check(serviceDispatchIn(&manager,404,kOwnLauncherId),"icon refresh");}
    void restore(const HomeBundle&)override{
        const Result rc=had_previous_
            ?ncmContentMetaDatabaseSet(&database,&key,previous_.data(),previous_.size())
            :ncmContentMetaDatabaseRemove(&database,&key);
        if(R_FAILED(rc) || R_FAILED(ncmContentMetaDatabaseCommit(&database)))detail_+="; metadata rollback failed";
    }
};
}
HomeResult install_own_home_launcher(const char* path,std::string& detail){
    detail.clear();FILE* file=path?std::fopen(path,"rb"):nullptr;
    if(!file){detail="HOME bundle unavailable";return HomeResult::Failed;}
    bool ok=std::fseek(file,0,SEEK_END)==0;const long bytes=std::ftell(file);
    ok=ok && bytes>0 && bytes<=16*1024*1024 && std::fseek(file,0,SEEK_SET)==0;
    std::vector<std::uint8_t> raw(ok?std::size_t(bytes):0);
    ok=ok && std::fread(raw.data(),1,raw.size(),file)==raw.size() && !std::ferror(file);
    if(std::fclose(file))ok=false;
    HomeBundle bundle;
    if(!ok || !parse_home_bundle(raw.data(),raw.size(),verify,bundle)){
        detail="HOME bundle validation failed";return HomeResult::Failed;
    }
    Backend backend(detail);const auto result=ensure_home(bundle,backend);
    if(result==HomeResult::Current)detail="HOME shortcut is current";
    if(result==HomeResult::Installed)detail="HOME shortcut installed";
    if(result==HomeResult::Failed && detail.empty())detail="HOME content validation or storage lookup failed";
    return result;
}
}
