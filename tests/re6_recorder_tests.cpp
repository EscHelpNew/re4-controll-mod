#include "re6/recorder.hpp"
#include "../source/re6_overlay/discovery.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
using namespace re6;
struct Fixture {
 ProducerRing<256> ring;Recorder recorder;std::string output;bool open_fail=false,write_fail=false,finish_fail=false,finalized=false,aborted=false,torn=false,restart=false,advance=false;
 static BuildId build(){BuildId b{};b[0]=1;return b;}
 static bool read(void* ctx,std::uint64_t address,void* out,std::size_t size){
  auto& f=*static_cast<Fixture*>(ctx);if(address<0x1000||size>sizeof(f.ring.storage)||address-0x1000>sizeof(f.ring.storage)-size)return false;
  std::memcpy(out,reinterpret_cast<const char*>(&f.ring.storage)+address-0x1000,size);
  if(size==sizeof(Record)&&f.torn){f.torn=false;const auto slot=(address-0x1000-sizeof(Header)-8)/sizeof(AtomicSlot);f.ring.storage.slots[slot].sequence.fetch_add(1);}
  if(size==sizeof(Record)&&f.restart){f.restart=false;f.ring.initialize(44,build());}
  if(size==sizeof(Record)&&f.advance)f.capture();
  return true;
 }
 static bool begin(void* ctx,const Header&){auto& f=*static_cast<Fixture*>(ctx);f.output.clear();f.finalized=f.aborted=false;return !f.open_fail;}
 static bool write(void* ctx,const char* bytes,std::size_t size){auto& f=*static_cast<Fixture*>(ctx);if(f.write_fail)return false;f.output.append(bytes,size);return true;}
 static bool finish(void* ctx,bool success){auto& f=*static_cast<Fixture*>(ctx);f.aborted=!success;f.finalized=success&&!f.finish_fail;return !f.finish_fail;}
 Fixture(){CHECK(ring.initialize(7,build()));}
 RemoteReader reader(){return {this,read};}
 RecorderSink sink(){return {this,begin,write,finish};}
 void capture(unsigned count=1){for(unsigned i=0;i<count;++i){Record r{};r.frame=++frame;r.validity=InputValid;r.sticks[0]=-123;CHECK(ring.capture(r));}}
 bool start(){return recorder.start(reader(),0x1000,build(),kTitleId,sink());}
 unsigned frame=0;
};
void starts_at_tail_and_saves_only_new_frames(){
 Fixture f;f.capture(300);CHECK(f.start());CHECK(f.output.find("#capture,start_after_sequence=300,capacity=256\n")!=std::string::npos);CHECK(f.recorder.state()==RecorderState::Recording);f.recorder.poll(32);CHECK(f.recorder.records()==0&&f.recorder.dropped()==0);
 f.capture(2);f.recorder.poll(32);CHECK(f.recorder.records()==2&&f.recorder.dropped()==0);CHECK(f.output.find(",301,0,301,")!=std::string::npos);
 CHECK(f.recorder.request_stop());f.recorder.poll(32);CHECK(f.recorder.state()==RecorderState::Saved&&f.finalized);CHECK(f.start());f.recorder.poll(32);CHECK(f.recorder.records()==0);
}
void stop_is_a_finite_snapshot_while_producer_continues(){
 Fixture f;CHECK(f.start());f.capture(10);CHECK(f.recorder.request_stop());f.advance=true;
 for(unsigned i=0;i<20&&f.recorder.active();++i)f.recorder.poll(1);
 CHECK(f.recorder.state()==RecorderState::Saved);CHECK(f.recorder.records()==10&&f.recorder.dropped()==0);CHECK(f.frame==20);
}
void ring_loss_is_counted_and_stop_does_not_export_future_frames(){
 Fixture f;CHECK(f.start());f.capture(300);CHECK(f.recorder.request_stop());f.capture(300);f.recorder.poll(4);
 CHECK(f.recorder.state()==RecorderState::Saved);CHECK(f.recorder.records()==0&&f.recorder.dropped()==300);
 Fixture g;CHECK(g.start());g.capture(300);g.recorder.poll(300);CHECK(g.recorder.records()==256&&g.recorder.dropped()==44);
}
void disk_failure_never_claims_saved(){
 Fixture f;CHECK(f.start());f.capture();f.write_fail=true;f.recorder.poll(8);CHECK(f.recorder.state()==RecorderState::Failed&&f.aborted);CHECK(f.recorder.error()==RecorderError::Write);
 Fixture g;CHECK(g.start());g.finish_fail=true;CHECK(g.recorder.request_stop());g.recorder.poll(8);CHECK(g.recorder.state()==RecorderState::Failed&&g.recorder.error()==RecorderError::Finalize);
 Fixture h;h.write_fail=true;CHECK(!h.start());CHECK(h.recorder.error()==RecorderError::Write&&h.aborted);
 Fixture i;i.open_fail=true;CHECK(!i.start());CHECK(i.recorder.error()==RecorderError::Open&&!i.finalized);
}
void restart_and_torn_headers_do_not_export_unverified_data(){
 Fixture f;CHECK(f.start());f.capture();f.restart=true;f.recorder.poll(8);CHECK(f.recorder.state()==RecorderState::Failed&&f.recorder.error()==RecorderError::Changed&&f.recorder.records()==0);
 Fixture g;CHECK(g.start());g.capture();const auto seq=g.ring.storage.header.sequence.load();g.ring.storage.header.sequence.store(seq+1);g.recorder.poll(8);CHECK(g.recorder.records()==0&&g.recorder.active());
 g.ring.storage.header.sequence.store(seq);g.recorder.poll(8);CHECK(g.recorder.records()==1);g.recorder.abort(RecorderError::Read);CHECK(g.aborted&&!g.finalized);
 Fixture h;CHECK(h.start());h.capture();h.torn=true;h.recorder.poll(8);CHECK(h.recorder.records()==0&&h.recorder.active());h.ring.storage.slots[0].sequence.store(2);h.recorder.poll(8);CHECK(h.recorder.records()==1);
 Fixture i;CHECK(i.start());i.capture();const auto before=i.ring.storage.header.sequence.load();i.ring.storage.header.sequence.store(before+1);CHECK(!i.recorder.request_stop()&&i.recorder.state()==RecorderState::Recording);i.ring.storage.header.sequence.store(before);CHECK(i.recorder.request_stop());i.capture();CHECK(i.recorder.request_stop());i.recorder.poll(8);CHECK(i.recorder.records()==1&&i.finalized);
 Fixture j;CHECK(j.start());j.ring.storage.header.sequence.fetch_add(1);for(unsigned n=0;n<250;++n)j.recorder.poll(8);CHECK(j.recorder.state()==RecorderState::Failed&&j.recorder.error()==RecorderError::Stalled);
}
void discovery_scans_bss_past_128k_and_across_chunk_boundary(){
 struct Region{std::vector<unsigned char> bytes;static bool read(void* p,std::uint64_t addr,void* dst,std::size_t size){auto& v=static_cast<Region*>(p)->bytes;if(addr<0x1000||size>v.size()||addr-0x1000>v.size()-size)return false;std::memcpy(dst,v.data()+addr-0x1000,size);return true;}};
 Region r;r.bytes.resize(0x80000);Fixture f;
 // The header begins at the last 8 bytes of a page: both the marker and header
 // span into the next chunk. A decoy marker must not be accepted as a ring.
 std::memcpy(r.bytes.data()+0x40+8,kJournalMarker,16);
 constexpr std::size_t offset=0x30FF8;std::memcpy(r.bytes.data()+offset,&f.ring.storage,sizeof(f.ring.storage));
 std::uint64_t examined=0;CHECK(telemetry::find_journal({&r,Region::read},0x1000,r.bytes.size(),Fixture::build(),kTitleId,examined)==0x31FF8);
 CHECK(examined>0x20000);auto wrong=Fixture::build();wrong[0]=3;examined=0;CHECK(!telemetry::find_journal({&r,Region::read},0x1000,r.bytes.size(),wrong,kTitleId,examined));
 examined=telemetry::kScanLimit;CHECK(!telemetry::find_journal({&r,Region::read},0x1000,r.bytes.size(),Fixture::build(),kTitleId,examined));
}
int main(){starts_at_tail_and_saves_only_new_frames();stop_is_a_finite_snapshot_while_producer_continues();ring_loss_is_counted_and_stop_does_not_export_future_frames();disk_failure_never_claims_saved();restart_and_torn_headers_do_not_export_unverified_data();discovery_scans_bss_past_128k_and_across_chunk_boundary();std::puts("Telemetry recorder tests passed");}
