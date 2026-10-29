#include "re4/update.hpp"
#include "re4/version.hpp"
#include "reload_project.h"
#include <switch.h>
#include <curl/curl.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <new>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace re4 {
int update_hash_file(const char* path,unsigned char* out){
    FILE* file=std::fopen(path,"rb");if(!file)return 0;
    Sha256Context hash;sha256ContextCreate(&hash);
    unsigned char buffer[16384];std::size_t bytes;
    while((bytes=std::fread(buffer,1,sizeof(buffer),file))>0)sha256ContextUpdate(&hash,buffer,bytes);
    bool ok=!std::ferror(file);
    if(std::fclose(file))ok=false;
    if(ok)sha256ContextGetHash(&hash,out);
    return ok?1:0;
}
struct UpdateWorker::Impl {
    mutable std::mutex mutex;
    UpdateSnapshot state;
    ReleaseInfo release;
    std::atomic<bool> running{false},cancelled{false};
    Thread thread{};
    bool created=false,ready=false,download=false;
    struct Transfer {
        Impl* owner;
        std::string text;
        FILE* file=nullptr;
        std::size_t limit=0,received=0;
    };
    void set(UpdateStage stage,const std::string& detail=""){
        std::lock_guard<std::mutex> lock(mutex);
        state.stage=stage;state.detail=detail;
    }
    static std::size_t write(char* data,std::size_t size,std::size_t count,void* context){
        auto& t=*static_cast<Transfer*>(context);
        if(size && count>t.limit/size)return 0;
        const auto bytes=size*count;
        if(t.owner->cancelled || bytes>t.limit-t.received)return 0;
        if(t.file){if(std::fwrite(data,1,bytes,t.file)!=bytes)return 0;}
        else t.text.append(data,bytes);
        t.received+=bytes;
        if(t.file){
            std::lock_guard<std::mutex> lock(t.owner->mutex);
            t.owner->state.received=std::uint32_t(t.received);
        }
        return bytes;
    }
    static int progress(void* context,curl_off_t,curl_off_t,curl_off_t,curl_off_t){
        return static_cast<Impl*>(context)->cancelled?1:0;
    }
    template<class T> static bool option(CURL* curl,CURLoption key,T value){
        return curl_easy_setopt(curl,key,value)==CURLE_OK;
    }
    bool fetch(const char* asset,Transfer& transfer){
        CURL* curl=curl_easy_init();
        if(!curl){set(UpdateStage::Failed,"HTTPS initialization failed");return false;}
        const std::string url=std::string(kUpdateBase)+asset;
        const bool configured=option(curl,CURLOPT_URL,url.c_str())
            && option(curl,CURLOPT_USERAGENT,"RE4-Controls-Updater")
            && option(curl,CURLOPT_FOLLOWLOCATION,1L) && option(curl,CURLOPT_MAXREDIRS,5L)
            && option(curl,CURLOPT_PROTOCOLS,long(CURLPROTO_HTTPS))
            && option(curl,CURLOPT_REDIR_PROTOCOLS,long(CURLPROTO_HTTPS))
            && option(curl,CURLOPT_SSL_VERIFYPEER,1L) && option(curl,CURLOPT_SSL_VERIFYHOST,2L)
            && option(curl,CURLOPT_SSLVERSION,long(CURL_SSLVERSION_TLSv1_2|CURL_SSLVERSION_MAX_TLSv1_2))
            && option(curl,CURLOPT_CONNECTTIMEOUT,10L) && option(curl,CURLOPT_TIMEOUT,transfer.file?300L:30L)
            && option(curl,CURLOPT_LOW_SPEED_LIMIT,10L) && option(curl,CURLOPT_LOW_SPEED_TIME,30L)
            && option(curl,CURLOPT_NOSIGNAL,1L) && option(curl,CURLOPT_FAILONERROR,1L)
            && option(curl,CURLOPT_WRITEFUNCTION,write) && option(curl,CURLOPT_WRITEDATA,&transfer)
            && option(curl,CURLOPT_NOPROGRESS,0L) && option(curl,CURLOPT_XFERINFOFUNCTION,progress)
            && option(curl,CURLOPT_XFERINFODATA,this);
        if(!configured){curl_easy_cleanup(curl);set(UpdateStage::Failed,"HTTPS options unavailable");return false;}
        const auto rc=curl_easy_perform(curl);
        long http=0;
        const auto info=curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);
        curl_easy_cleanup(curl);
        if(cancelled){set(UpdateStage::Cancelled);return false;}
        if(rc!=CURLE_OK || info!=CURLE_OK || http!=200){
            char detail[96];std::snprintf(detail,sizeof(detail),"HTTP %ld / HTTPS error %u",http,unsigned(rc));
            set(UpdateStage::Failed,detail);return false;
        }
        return true;
    }
    static int hash_file(const char* path,unsigned char* out){return update_hash_file(path,out);}
    void check_release(){
        Transfer transfer{this,{},nullptr,512,0};
        if(!fetch("re4-controls-update.txt",transfer))return;
        ReleaseInfo info;
        if(!parse_release_manifest(transfer.text,info)){set(UpdateStage::Failed,"Invalid release manifest");return;}
        std::lock_guard<std::mutex> lock(mutex);
        release=info;state.version=info.version;state.total=info.bytes;
        state.stage=newer_version(info.version,kVersion)?UpdateStage::Available:UpdateStage::Current;
    }
    void download_release(){
        const std::string path=std::string(NXDK_SD_ROOT)+"/update.nro.download";
        FILE* file=std::fopen(path.c_str(),"wb");
        if(!file){set(UpdateStage::Failed,"Could not create update download");return;}
        Transfer transfer{this,{},file,release.bytes,0};
        bool ok=fetch("re4-controls.nro",transfer);
        if(std::fflush(file))ok=false;
#ifdef _WIN32
        if(_commit(_fileno(file)))ok=false;
#else
        if(fsync(fileno(file)))ok=false;
#endif
        if(std::fclose(file))ok=false;
        if(cancelled){ok=false;set(UpdateStage::Cancelled);}
        if(!ok){
            if(!cancelled){
                std::lock_guard<std::mutex> lock(mutex);
                if(state.stage!=UpdateStage::Failed){state.stage=UpdateStage::Failed;state.detail="Could not finish update download";}
            }
            std::remove(path.c_str());return;
        }
        set(UpdateStage::Installing);
        UpdateFileOps ops;std::string detail;
        const bool installed=publish_update(NXDK_SD_ROOT,release,hash_file,ops,detail);
        set(installed?UpdateStage::Installed:UpdateStage::Failed,detail);
    }
    static void entry(void* context){
        auto& self=*static_cast<Impl*>(context);
        if(self.download)self.download_release();else self.check_release();
        self.running=false;
    }
    bool start(bool install){
        if(running)return false;
        if(created){threadWaitForExit(&thread);threadClose(&thread);created=false;}
        if(!ready){set(UpdateStage::Failed,"HTTPS service unavailable");return false;}
        {
            std::lock_guard<std::mutex> lock(mutex);
            if(install && state.stage!=UpdateStage::Available)return false;
            download=install;cancelled=false;state.received=0;state.detail.clear();
            state.stage=install?UpdateStage::Downloading:UpdateStage::Checking;
        }
        running=true;
        Result rc=threadCreate(&thread,entry,this,nullptr,128*1024,0x2C,-2);
        if(R_SUCCEEDED(rc)){created=true;rc=threadStart(&thread);}
        if(R_FAILED(rc)){
            running=false;
            if(created){threadClose(&thread);created=false;}
            set(UpdateStage::Failed,"Could not start update worker");return false;
        }
        return true;
    }
};
UpdateWorker::UpdateWorker():impl_(new(std::nothrow) Impl){
    if(impl_)impl_->ready=curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK;
}
UpdateWorker::~UpdateWorker(){close();}
void UpdateWorker::close(){
    if(!impl_)return;
    impl_->cancelled=true;
    if(impl_->created){threadWaitForExit(&impl_->thread);threadClose(&impl_->thread);}
    if(impl_->ready)curl_global_cleanup();
    delete impl_;impl_=nullptr;
}
bool UpdateWorker::check(){return impl_ && impl_->start(false);}
bool UpdateWorker::install(){return impl_ && impl_->start(true);}
void UpdateWorker::cancel(){if(impl_)impl_->cancelled=true;}
bool UpdateWorker::busy()const{return impl_ && impl_->running;}
UpdateSnapshot UpdateWorker::snapshot()const{
    if(!impl_){UpdateSnapshot state;state.stage=UpdateStage::Failed;state.detail="Updater unavailable";return state;}
    std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->state;
}
}
