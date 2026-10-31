#include "re6/recorder.hpp"
#include "re6/profile.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
struct Fixture {
 re6::ProducerRing<256> ring;re6::Recorder recorder;FILE* file=nullptr;
 static bool read(void* p,std::uint64_t address,void* out,std::size_t size){
  const auto& ring=static_cast<Fixture*>(p)->ring.storage;
  const auto base=reinterpret_cast<std::uintptr_t>(&ring);
  if(address<base||size>sizeof(ring)||address-base>sizeof(ring)-size)return false;
  std::memcpy(out,reinterpret_cast<const void*>(address),size);return true;
 }
 static bool begin(void* p,const re6::Header&){return static_cast<Fixture*>(p)->file!=nullptr;}
 static bool write(void* p,const char* text,std::size_t count){return std::fwrite(text,1,count,static_cast<Fixture*>(p)->file)==count;}
 static bool finish(void* p,bool ok){auto& f=*static_cast<Fixture*>(p);return ok&&std::fprintf(f.file,"#end,records=%llu,dropped=%llu,status=complete\n",(unsigned long long)f.recorder.records(),(unsigned long long)f.recorder.dropped())>0;}
};
int main(int argc,char** argv){
 if(argc!=2)return 2;Fixture f;f.file=std::fopen(argv[1],"wb");if(!f.file)return 3;
 f.ring.initialize(4,re6::profile::build_id);
 const auto base=reinterpret_cast<std::uintptr_t>(&f.ring.storage);
 if(!f.recorder.start({&f,Fixture::read},base,re6::profile::build_id,re6::kTitleId,{&f,Fixture::begin,Fixture::write,Fixture::finish}))return 4;
 for(unsigned i=1;i<=2;++i){
  re6::Record r{};r.frame=r.sample_id=i;r.tick=i*640000;r.input_tick=r.tick-100;
  r.actor=4096;r.camera=8192;r.controller_style=1;r.validity=15;r.sticks[2]=32767;
  r.player[0]=r.player[5]=r.player[10]=1;r.player[3]=float((i-1)*2);
  const float angle=float((i-1)*3.14159265358979323846/90);
  r.view[0]=r.view[10]=std::cos(angle);r.view[2]=-std::sin(angle);r.view[8]=std::sin(angle);r.view[5]=1;
  r.projection[0]=r.projection[5]=r.projection[10]=r.projection[15]=1;
  f.ring.capture(r);f.recorder.poll();
 }
 if(!f.recorder.request_stop())return 5;f.recorder.poll();
 const bool ok=f.recorder.state()==re6::RecorderState::Saved;
 return std::fclose(f.file)||!ok?6:0;
}
