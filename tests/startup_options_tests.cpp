#include "re4/startup_options.hpp"
#include <cassert>
#include "re4/startup_updates.hpp"
#include <cstdio>
namespace {
int constructions=0;
struct CountingUpdater {
 CountingUpdater(){++constructions;}
 bool unavailable=false;
 re4::UpdateSnapshot snapshot() const {
  re4::UpdateSnapshot result;
  if(unavailable){result.stage=re4::UpdateStage::Failed;result.detail="Updater unavailable";return result;}
  result.stage=re4::UpdateStage::Available;
  result.version="9.0.0";result.detail="Release ready";return result;
 }
};
}
int main(){
 assert(!re4::startup_options(0,nullptr).recovery);
 const char* normal[]={"re4-controls.nro","--game-remote=local","--token=--recovery"};
 assert(!re4::startup_options(3,normal).recovery);
 const char* recovery[]={"re4-controls.nro","--recovery"};
 assert(re4::startup_options(2,recovery).recovery);
 assert(!re4::startup_options(1,recovery+1).recovery);
 const char* sparse[]={"re4-controls.nro",nullptr,"--recovery-other"};
 assert(!re4::startup_options(3,sparse).recovery);
 const char* mixed[]={"re4-controls.nro",nullptr,"--recovery"};
 assert(re4::startup_options(3,mixed).recovery);

 int failures=0;
 const auto require=[&](bool ok,const char* reason){if(!ok){std::fprintf(stderr,"FAIL: %s\n",reason);++failures;}};
 const auto idle=re4::startup_update_snapshot<CountingUpdater>(nullptr,re4::Language::English,{false});
 require(idle.stage==re4::UpdateStage::Idle,"normal startup without a requested updater must remain Idle");
 require(idle.detail.find("recovery mode")==std::string::npos,"idle normal startup must not report recovery restrictions");
 require(constructions==0,"idle snapshots must not construct network workers");
 auto recovery_updater=re4::create_startup_updater<CountingUpdater>({true});
 require(!recovery_updater && constructions==0,"recovery must not construct a network updater");
 for(unsigned frame=0;frame<3;++frame){
  const auto disabled=re4::startup_update_snapshot(recovery_updater.get(),re4::Language::English,{true});
  require(disabled.stage==re4::UpdateStage::Failed,"every recovery frame must publish an unavailable update state");
  require(disabled.detail.find("recovery mode")!=std::string::npos,"Updates must explain the recovery restriction in English");
 }
 const auto russian=re4::startup_update_snapshot(recovery_updater.get(),re4::Language::Russian,{true});
 require(russian.detail.find(u8"\u0432\u043e\u0441\u0441\u0442\u0430\u043d\u043e\u0432\u043b\u0435\u043d\u0438\u044f")!=std::string::npos,"changing language must publish the Russian recovery reason");
 const auto before_normal=constructions;
 auto normal_updater=re4::create_startup_updater<CountingUpdater>({false});
 require(normal_updater && constructions==before_normal+1,"normal startup must construct exactly one updater");
 const auto available=re4::startup_update_snapshot(normal_updater.get(),re4::Language::Russian,{false});
 require(available.stage==re4::UpdateStage::Available && available.version=="9.0.0" && available.detail=="Release ready",
   "normal update snapshots must retain worker data without replacement or translation");
 require(constructions==before_normal+1,"polling snapshots must not construct another updater");
 normal_updater->unavailable=true;
 const auto unavailable=re4::startup_update_snapshot(normal_updater.get(),re4::Language::English,{false});
 require(unavailable.stage==re4::UpdateStage::Failed && unavailable.detail=="Updater unavailable",
   "normal updater initialization failures must retain the actual worker reason");
 if(!failures)std::puts("PASS: startup parsing, normal-only updater construction, persistent localized update state");
 return failures?1:0;
}
