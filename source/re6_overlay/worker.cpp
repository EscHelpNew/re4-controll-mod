#include "worker.hpp"
#include "discovery.hpp"
#include "re4/profile.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace telemetry {
namespace {
bool mounted=false;
struct Extents {u64 base,size;};
struct Metadata {u64 pid,title;Extents main,heap,alias,space;u8 build_id[32];};
static_assert(sizeof(Metadata)==112);
re6::BuildId expected_build(Target target){
 re6::BuildId build{};
 if(target==Target::RE4)std::memcpy(build.data(),re4::profile::build_id,32);
 else {const std::uint8_t fixed[16]={0x06,0x1D,0x2A,0x06,0xCD,0x70,0x07,0x85,0xE9,0x87,0xC4,0x2C,0xE2,0xD7,0x22,0x31};std::memcpy(build.data(),fixed,16);}
 return build;
}
}
bool mount_storage(){
 if(mounted)return true;
 FsFileSystem fs{};if(R_FAILED(fsOpenSdCardFileSystem(&fs)))return false;
 mounted=fsdevMountDevice("r6log",fs)>=0;
 if(mounted){mkdir("r6log:/switch",0777);mkdir("r6log:/switch/re4-controls",0777);mkdir(kDirectory,0777);mkdir("r6log:/config",0777);}
 return mounted;
}
void close_storage(){if(mounted)fsdevUnmountDevice("r6log");mounted=false;}
bool Worker::read(void* context,std::uint64_t address,void* out,std::size_t count){
 auto& self=*static_cast<Worker*>(context);const struct {u64 address,size;} in={address,count};
 self.last_result_=serviceDispatchIn(&self.service_,65102,in,
  .buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_Out},.buffers={{out,count}});
 if(R_FAILED(self.last_result_)){
  const bool scanning=self.diagnostic_.phase==re6::CapturePhase::Scan;
  if(!scanning)self.diagnostic_.phase=re6::CapturePhase::Read;
  self.publish(scanning?"Memory scan read failed":"Game memory read failed",scanning);
 }
 return R_SUCCEEDED(self.last_result_);
}
bool Worker::cancelled(void* context){auto& self=*static_cast<Worker*>(context);return self.exit_.load()||self.command_.load()==3;}
void Worker::publish(const char* detail,bool busy,bool force_diagnostic){
 mutexLock(&mutex_);
 const bool changed=snapshot_.state!=recorder_.state()||snapshot_.error!=recorder_.error()||snapshot_.result!=last_result_||(detail&&std::strcmp(snapshot_.detail,detail));
 snapshot_.state=recorder_.state();snapshot_.error=recorder_.error();
 snapshot_.records=recorder_.records();snapshot_.dropped=recorder_.dropped();snapshot_.errors=errors_;snapshot_.result=last_result_;snapshot_.busy=busy;
 if(detail)std::snprintf(snapshot_.detail,sizeof(snapshot_.detail),"%s",detail);
 const auto current=snapshot_;mutexUnlock(&mutex_);
 if(diagnostic_requested_&&(changed||force_diagnostic)){
  diagnostic_.state=current.state;diagnostic_.error=current.error;diagnostic_.ipc_result=current.result;
  diagnostic_.records=current.records;diagnostic_.dropped=current.dropped;diagnostic_.errors=current.errors;
  diagnostic_.detail=current.detail;diagnostic_.filename=current.filename;diagnostic_.tick=armGetSystemTick();
  const bool ok=mounted&&re6::write_capture_status("r6log:/switch/re4-controls/telemetry/telemetry-status.csv",diagnostic_);
  mutexLock(&mutex_);snapshot_.diagnostic_failed=!ok;mutexUnlock(&mutex_);
  diagnostic_.detail="";diagnostic_.filename="";
 }
}
bool Worker::connect(Target target,std::uint64_t& address,re6::BuildId& build,std::uint64_t& title){
 title=target==Target::RE6?re6::kTitleId:re4::profile::title_id;build=expected_build(target);
 diagnostic_.phase=re6::CapturePhase::Attach;
 if(!serviceIsActive(&service_)){publish("Debug service unavailable");return false;}
 last_result_=serviceDispatch(&service_,65003);
 if(R_FAILED(last_result_)){publish("Start the selected game; attach failed");return false;}
 diagnostic_.phase=re6::CapturePhase::Metadata;
 Metadata meta{};last_result_=serviceDispatchOut(&service_,65002,meta);
 if(R_SUCCEEDED(last_result_)){diagnostic_.actual_title=meta.title;diagnostic_.pid=meta.pid;std::memcpy(diagnostic_.actual_build.data(),meta.build_id,32);}
 if(R_FAILED(last_result_)||meta.title!=title||std::memcmp(meta.build_id,build.data(),32)){
  publish("Unsupported game or build ID");return false;
 }
 std::array<MemoryInfo,32> maps{};std::uint64_t examined=0;
 for(u64 offset=0;offset<8192&&!cancelled(this);){
  diagnostic_.phase=re6::CapturePhase::Maps;
  u64 count=0;last_result_=serviceDispatchInOut(&service_,65101,offset,count,
   .buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_Out},.buffers={{maps.data(),sizeof(maps)}});
  if(R_FAILED(last_result_)||count>maps.size()){publish("Memory map query failed");return false;}
  if(!count)break;
  for(u64 i=0;i<count&&!cancelled(this);++i){
   const auto& map=maps[i];if((map.type&0xFF)!=MemType_CodeMutable||!(map.perm&Perm_R))continue;
   diagnostic_.phase=re6::CapturePhase::Scan;
   address=find_journal({this,read},map.addr,map.size,build,title,examined,cancelled,this);
   if(address)return true;
   if(examined>=kScanLimit){publish("Memory scan limit reached");return false;}
  }
  offset+=count;
 }
 publish(cancelled(this)?"Start cancelled":"Journal not found; install module and restart game");return false;
}
bool Worker::begin(void* context,const re6::Header& header){
 auto& self=*static_cast<Worker*>(context);
 if(!mounted)return false;
 char name[96];std::snprintf(name,sizeof(name),"%016llX-%llu-%llu.csv",
  (unsigned long long)header.title_id,(unsigned long long)std::time(nullptr),(unsigned long long)armGetSystemTick());
 std::snprintf(self.final_,sizeof(self.final_),"%s/%s",kDirectory,name);
 std::snprintf(self.part_,sizeof(self.part_),"%s/%s.part",kDirectory,name);
 const int descriptor=open(self.part_,O_WRONLY|O_CREAT|O_EXCL,0666);
 if(descriptor<0)return false;
 self.file_=fdopen(descriptor,"wb");
 if(!self.file_){::close(descriptor);return false;}
 setvbuf(self.file_,self.file_buffer_,_IOFBF,sizeof(self.file_buffer_));self.last_flush_=armGetSystemTick();
 mutexLock(&self.mutex_);std::snprintf(self.snapshot_.filename,sizeof(self.snapshot_.filename),"%s.part",name);mutexUnlock(&self.mutex_);
 return true;
}
bool Worker::write(void* context,const char* data,std::size_t count){
 auto& self=*static_cast<Worker*>(context);
 if(!self.file_||std::fwrite(data,1,count,self.file_)!=count){self.diagnostic_.phase=re6::CapturePhase::Write;return false;}
 const auto now=armGetSystemTick();
 if(now-self.last_flush_>=re6::kTickHz){self.last_flush_=now;if(std::fflush(self.file_)||fsync(fileno(self.file_))){self.diagnostic_.phase=re6::CapturePhase::Write;return false;}}
 return true;
}
bool Worker::finish(void* context,bool success){
 auto& self=*static_cast<Worker*>(context);if(!self.file_)return false;
 bool ok=std::fprintf(self.file_,"#end,records=%llu,dropped=%llu,status=%s\n",
  (unsigned long long)self.recorder_.records(),(unsigned long long)self.recorder_.dropped(),success?"complete":"incomplete")>0;
 if(std::fflush(self.file_))ok=false;
 if(fsync(fileno(self.file_)))ok=false;
 if(std::fclose(self.file_))ok=false;
 self.file_=nullptr;
 if(success&&ok)ok=std::rename(self.part_,self.final_)==0;
 if(success&&ok){mutexLock(&self.mutex_);const auto* name=std::strrchr(self.final_,'/');std::snprintf(self.snapshot_.filename,sizeof(self.snapshot_.filename),"%.127s",name?name+1:self.final_);mutexUnlock(&self.mutex_);}
 if(!ok)self.diagnostic_.phase=re6::CapturePhase::Write;
 return ok;
}
void Worker::begin_recording(Target target){
 diagnostic_=re6::CaptureStatus{};diagnostic_requested_=true;
 diagnostic_.target=target==Target::RE6?"RE6":"RE4";diagnostic_.target_title=target==Target::RE6?re6::kTitleId:re4::profile::title_id;
 diagnostic_.expected_build=expected_build(target);diagnostic_.phase=re6::CapturePhase::Start;
 recorder_=re6::Recorder{};
 mutexLock(&mutex_);snapshot_.filename[0]=0;mutexUnlock(&mutex_);
 publish("Looking for native journal",true,true);std::uint64_t address=0,title=0;re6::BuildId build{};
 if(!connect(target,address,build,title)){recorder_.abort(re6::RecorderError::Attach);++errors_;publish();return;}
 diagnostic_.phase=re6::CapturePhase::Start;recorder_.start({this,read},address,build,title,{this,begin,write,finish});
 if(recorder_.state()==re6::RecorderState::Failed)++errors_;
 publish(recorder_.active()?"Recording continues when overlay is hidden":"Could not start recording");
}
void Worker::entry(void* context){static_cast<Worker*>(context)->run();}
void Worker::run(){
 const bool sm_ready=R_SUCCEEDED(smInitialize());
 if(sm_ready){last_result_=smGetService(&service_,"dmnt:cht");smExit();}
 bool stopping=false;unsigned stop_retries=0;
 while(!exit_.load()||recorder_.active()){
  const auto command=command_.exchange(0);
  if(command==1||command==2){if(!recorder_.active()&&!exit_)begin_recording(command==1?Target::RE6:Target::RE4);}
  const auto before=recorder_.state();
  if(command==3||exit_){stopping=true;if(command==3){diagnostic_.phase=re6::CapturePhase::Stop;publish(nullptr,false,true);}}
  if(stopping&&recorder_.state()==re6::RecorderState::Recording){
   if(recorder_.request_stop()){stopping=false;stop_retries=0;}
   else if(++stop_retries>=250)recorder_.abort(re6::RecorderError::Stalled);
  }
  if(!recorder_.active()){stopping=false;stop_retries=0;}
  if(!stopping&&recorder_.active())diagnostic_.phase=re6::CapturePhase::Read;
  recorder_.poll(128);
  if(before!=re6::RecorderState::Failed&&recorder_.state()==re6::RecorderState::Failed)++errors_;
  publish(before!=recorder_.state()&&recorder_.state()==re6::RecorderState::Saved?"Recording saved":before!=recorder_.state()&&recorder_.state()==re6::RecorderState::Failed?"Recording failed; partial file retained":nullptr);
  svcSleepThread(4'000'000);
 }
 if(serviceIsActive(&service_))serviceClose(&service_);
}
bool Worker::initialize(){
 mutexInit(&mutex_);exit_=false;command_=0;
 const auto created=threadCreate(&thread_,entry,this,nullptr,64*1024,0x2C,-2);
 if(R_FAILED(created)){last_result_=created;++errors_;publish("Could not create recorder worker");return false;}
 created_=true;const auto started=threadStart(&thread_);
 if(R_FAILED(started)){threadClose(&thread_);created_=false;last_result_=started;++errors_;publish("Could not start recorder worker");return false;}
 return true;
}
void Worker::close(){if(!created_)return;exit_=true;threadWaitForExit(&thread_);threadClose(&thread_);created_=false;}
bool Worker::start(Target target){if(!created_)return false;const auto state=snapshot();if(state.busy||state.state==re6::RecorderState::Recording||state.state==re6::RecorderState::Stopping)return false;unsigned empty=0;return command_.compare_exchange_strong(empty,target==Target::RE6?1u:2u);}
void Worker::stop(){if(created_)command_=3;}
Snapshot Worker::snapshot(){mutexLock(&mutex_);const auto copy=snapshot_;mutexUnlock(&mutex_);return copy;}
}
