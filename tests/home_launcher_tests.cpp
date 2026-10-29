#include "re4/home_launcher.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
struct Fake:re4::HomeBackend{
    int same=0,fail=0,puts=0,begins=0,publishes=0,restores=0,records=0,refreshes=0;
    bool begin()override{++begins;return fail!=1;}
    int current(const re4::HomeBundle&)override{return same;}
    bool put(const re4::HomeContent&)override{return ++puts!=fail;}
    bool snapshot(const re4::HomeBundle&)override{return fail!=4;}
    bool publish(const re4::HomeBundle&)override{++publishes;return fail!=5;}
    bool register_record(const re4::HomeBundle&)override{++records;return fail!=6;}
    bool refresh()override{++refreshes;return fail!=7;}
    void restore(const re4::HomeBundle&)override{++restores;}
};
int main(){
    using namespace re4;
    HomeBundle bundle;bundle.title=kOwnLauncherId;
    Fake backend;assert(ensure_home(bundle,backend)==HomeResult::Installed);
    assert(backend.puts==3 && backend.publishes==1 && backend.records==1 && backend.refreshes==1);
    Fake current;current.same=1;
    assert(ensure_home(bundle,current)==HomeResult::Current && current.puts==0 && current.refreshes==1);
    for(int failure=1;failure<=6;++failure){
        Fake broken;broken.fail=failure;
        assert(ensure_home(bundle,broken)==HomeResult::Failed);
        if(failure<5)assert(broken.publishes==0);
        else assert(broken.restores==1);
    }
    Fake refresh;refresh.fail=7;
    assert(ensure_home(bundle,refresh)==HomeResult::NeedsRefresh && refresh.publishes==1 && refresh.restores==0);
    Fake foreign;bundle.title=0x010099A00BC1E000;
    assert(ensure_home(bundle,foreign)==HomeResult::Failed && foreign.begins==0);
    std::puts("own HOME transaction: idempotence, own title, staged content, rollback and cache refresh PASS");
}
