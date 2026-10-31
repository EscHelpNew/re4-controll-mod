#include "re6/journal.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
using namespace re6;
struct Memory {
 ProducerRing<256> ring;
 std::uint64_t base=0x100000;
 unsigned calls=0;bool fail=false,torn=false,restart=false;
 static bool read(void* p,std::uint64_t address,void* out,std::size_t count){
  auto& m=*static_cast<Memory*>(p);++m.calls;
  if(address<m.base||count>sizeof(m.ring.storage)||address-m.base>sizeof(m.ring.storage)-count)return false;
  if(m.fail&&count==sizeof(Record)){std::memset(out,0xAB,count/2);return false;}
  std::memcpy(out,reinterpret_cast<const unsigned char*>(&m.ring.storage)+(address-m.base),count);
  if(count==sizeof(Record)&&m.torn){m.torn=false;m.ring.capture(sample(257));}
  if(count==sizeof(Record)&&m.restart){m.restart=false;m.ring.initialize(99,build());}
  return true;
 }
 RemoteReader reader(){return {this,read};}
 static BuildId build(){BuildId id{};id[0]=0xA1;id[31]=0xF2;return id;}
 static Record sample(std::uint64_t n){
  Record r{};r.frame=n;r.tick=n*640000;r.input_tick=r.tick-123;r.sample_id=n*2;r.buttons=0x8000000000000010ULL;r.actor=0x123456789ABCULL;r.camera=0xFEDCBA987654ULL;
  r.controller_id=32;r.controller_style=4;r.validity=InputValid|PlayerValid|CameraValid|ProjectionValid;
  r.sticks[0]=-32768;r.sticks[1]=32767;r.sticks[2]=-123;r.sticks[3]=456;
  r.player[0]=r.player[5]=r.player[10]=1;r.player[3]=12.25f;r.player[7]=-30.5f;r.player[11]=99;
  r.view[0]=r.view[5]=r.view[10]=1;r.view[3]=-3;r.view[7]=-4;r.view[11]=-5;
  r.projection[0]=.75f;r.projection[5]=1;r.projection[10]=-.001001f;r.projection[11]=-100.1001f;r.projection[14]=-1;
  return r;
 }
 Memory(){CHECK(ring.initialize(7,build()));}
};
void wrap_without_loss(){
 Memory m;Cursor c;Sample out{};CHECK(c.attach(m.reader(),m.base,Memory::build())==ReadStatus::Empty);
 CHECK(c.read_next(m.reader(),out)==ReadStatus::Empty);
 for(unsigned i=1;i<=700;++i){CHECK(m.ring.capture(Memory::sample(i)));CHECK(c.read_next(m.reader(),out)==ReadStatus::Record);CHECK(out.sequence==i&&out.record.frame==i&&out.missed==0);}
 CHECK(c.header().overwrites==444&&c.missed()==0);
}
void loss_and_attach(){
 Memory m;Cursor c;Sample out{};CHECK(c.attach(m.reader(),m.base,Memory::build())==ReadStatus::Empty);
 m.ring.capture(Memory::sample(1));CHECK(c.read_next(m.reader(),out)==ReadStatus::Record);
 for(unsigned i=2;i<=300;++i)m.ring.capture(Memory::sample(i));
 CHECK(c.read_next(m.reader(),out)==ReadStatus::Record);CHECK(out.sequence==45&&out.record.frame==45&&out.missed==43);
 Cursor late;CHECK(late.attach(m.reader(),m.base,Memory::build())==ReadStatus::Empty);CHECK(late.read_next(m.reader(),out)==ReadStatus::Record);CHECK(out.sequence==45&&out.missed==44);
}
void interleaving(){
 Memory m;for(unsigned i=1;i<=256;++i)m.ring.capture(Memory::sample(i));Cursor c;Sample out{};out.sequence=999;
 CHECK(c.attach(m.reader(),m.base,Memory::build())==ReadStatus::Empty);m.torn=true;
 CHECK(c.read_next(m.reader(),out)==ReadStatus::Torn&&out.sequence==999);
 CHECK(c.read_next(m.reader(),out)==ReadStatus::Record&&out.sequence==2&&out.missed==1);
 m.fail=true;const auto before=out;CHECK(c.read_next(m.reader(),out)==ReadStatus::IoError);CHECK(!std::memcmp(&out,&before,sizeof(out)));m.fail=false;
 m.restart=true;CHECK(c.read_next(m.reader(),out)==ReadStatus::Changed);CHECK(c.read_next(m.reader(),out)==ReadStatus::Detached);
 CHECK(c.attach(m.reader(),m.base,Memory::build())==ReadStatus::Empty);CHECK(c.header().session==99);
 m.ring.storage.header.sequence.store(99);CHECK(c.read_next(m.reader(),out)==ReadStatus::Torn);
}
void bounds_and_identity(){
 Memory m;Cursor c;auto wrong=Memory::build();wrong[0]^=1;
 CHECK(c.attach(m.reader(),m.base,wrong)==ReadStatus::Unsupported);
 CHECK(!m.ring.initialize(7,Memory::build()));
 const auto calls=m.calls;CHECK(c.attach(m.reader(),UINT64_MAX-16,Memory::build())==ReadStatus::Bounds);CHECK(m.calls==calls);
 // A corrupt remote capacity must be rejected before any slot address/read.
 const auto original=m.ring.storage.header.words[4].load();m.ring.storage.header.words[4].store((original&0xFFFFFFFF00000000ULL)|0xFFFFFFFFu);
 CHECK(c.attach(m.reader(),m.base,Memory::build())==ReadStatus::Unsupported);
 CHECK(!m.ring.initialize(0,Memory::build()));BuildId zero{};CHECK(!m.ring.initialize(1,zero));
 ProducerRing<1024> larger;CHECK(larger.initialize(123,Memory::build()));CHECK(larger.capture(Memory::sample(1)));
}
void invalid_and_csv(){
 Memory m;Cursor c;Sample out{};CHECK(c.attach(m.reader(),m.base,Memory::build())==ReadStatus::Empty);
 auto r=Memory::sample(42);r.player[3]=std::numeric_limits<float>::quiet_NaN();r.view[0]=std::numeric_limits<float>::infinity();r.validity&=~InputValid;
 CHECK(m.ring.capture(r));CHECK(c.read_next(m.reader(),out)==ReadStatus::Record);
 CHECK(!(out.record.validity&PlayerValid)&&!(out.record.validity&CameraValid)&&!(out.record.validity&InputValid));CHECK(out.record.validity&PlayerNonFinite);CHECK(out.record.validity&CameraNonFinite);
 char text[kCsvBufferBytes];const auto count=csv_header(c.header(),text,sizeof(text));CHECK(count>0);std::string header(text,count);
 CHECK(header.find("tick_hz=19200000")!=std::string::npos&&header.find("session=7")!=std::string::npos&&header.find("A1000000")!=std::string::npos);
 CHECK(header.find("player_m03")!=std::string::npos&&header.find("view_m03")!=std::string::npos&&header.find("projection_m33")!=std::string::npos);
 const auto bytes=csv_row(c.header(),out,text,sizeof(text));CHECK(bytes>0);const std::string row(text,bytes);
 CHECK(row.find("-32768,32767,-123,456")==std::string::npos);CHECK(row.rfind("7,1,0,42,26880000,,,,,",0)==0);
 CHECK(row.find("nan")==std::string::npos&&row.find("inf")==std::string::npos);CHECK(row.find(",,,,,,,,,,,,")!=std::string::npos);
 CHECK(row.substr(0,6)=="7,1,0,");CHECK(row.back()=='\n');
 char small[8]={'x'};CHECK(csv_row(c.header(),out,small,sizeof(small))==0&&small[0]==0);
 Header other=c.header();other.session=123;CHECK(csv_row(other,out,text,sizeof(text))==0&&text[0]==0);
 other=c.header();other.build_id[0]^=1;CHECK(csv_row(other,out,text,sizeof(text))==0&&text[0]==0);

 // Even a direct untrusted record cannot export NaN as valid CSV data.
 out.record=Memory::sample(2);out.record.projection[0]=std::numeric_limits<float>::quiet_NaN();CHECK(csv_row(c.header(),out,text,sizeof(text))>0);CHECK(std::strstr(text,"nan")==nullptr);
 CHECK(std::strstr(text,"-32768,32767,-123,456")!=nullptr); // Valid raw input remains exact.
}
void shared_titles_and_record_start(){
 Memory m;CHECK(!m.ring.initialize(8,Memory::build(),42));
 CHECK(m.ring.initialize(8,Memory::build(),kRe4TitleId));
 for(unsigned i=1;i<=700;++i)CHECK(m.ring.capture(Memory::sample(i)));
 Cursor c;Sample out{};
 CHECK(c.attach(m.reader(),m.base,Memory::build())==ReadStatus::Unsupported);
 CHECK(c.attach(m.reader(),m.base,Memory::build(),kRe4TitleId,true)==ReadStatus::Empty);
 CHECK(c.read_next(m.reader(),out)==ReadStatus::Empty&&c.missed()==0);
 CHECK(m.ring.capture(Memory::sample(701)));
 CHECK(c.read_next(m.reader(),out)==ReadStatus::Record);
 CHECK(out.sequence==701&&out.record.frame==701&&out.record.input_tick==701*640000-123&&out.missed==0&&out.title_id==kRe4TitleId);
 char text[kCsvBufferBytes];CHECK(csv_row(c.header(),out,text,sizeof(text))>0);
 out.title_id=kTitleId;CHECK(csv_row(c.header(),out,text,sizeof(text))==0);
}
int main(){static_assert(sizeof(Record)==256&&sizeof(Header)==128&&sizeof(AtomicSlot)==264);wrap_without_loss();loss_and_attach();interleaving();bounds_and_identity();invalid_and_csv();shared_titles_and_record_start();std::puts("RE6 bounded journal tests passed");}
