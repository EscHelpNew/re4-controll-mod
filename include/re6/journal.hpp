#pragma once
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace re6 {
inline constexpr std::uint32_t kJournalAbi=1,kTickHz=19200000;
inline constexpr std::uint64_t kTitleId=0x01002A000CD48000ULL;
inline constexpr char kJournalMarker[16]="RE6OBS_JOURNAL1";
inline constexpr std::uint64_t kMaxSequence=(UINT64_MAX-4)/2;
inline constexpr std::size_t kCsvBufferBytes=4096;
using BuildId=std::array<std::uint8_t,32>;
enum Validity : std::uint32_t {
 InputValid=1u,PlayerValid=2u,CameraValid=4u,ProjectionValid=8u,
 PlayerNonFinite=1u<<8,CameraNonFinite=1u<<9,ProjectionNonFinite=1u<<10
};
// All matrices retain native units and row order; no inferred world scale or
// Euler convention. Missing groups are identified by validity, not fake zeros.
struct alignas(8) Record {
 std::uint64_t frame=0,tick=0,sample_id=0,buttons=0,actor=0,camera=0;
 std::uint32_t controller_id=0,controller_style=0,validity=0,reserved=0;
 std::int32_t sticks[4]{};
 float player[12]{},view[12]{},projection[16]{};
 std::uint64_t reserved_tail[2]{};
};
struct alignas(8) Header {
 std::uint64_t sequence=0;
 char marker[16]{};
 std::uint32_t abi=0,header_size=0,record_size=0,slot_size=0,capacity=0,tick_hz=0;
 std::uint64_t session=0;
 BuildId build_id{};
 std::uint64_t published=0,overwrites=0,title_id=0;
 std::uint32_t flags=0,reserved=0;
 std::uint64_t reserved_tail=0;
};
struct AtomicHeader {
 std::atomic<std::uint64_t> sequence{0};
 std::array<std::atomic<std::uint64_t>,15> words{};
};
struct AtomicSlot {
 std::atomic<std::uint64_t> sequence{0};
 std::array<std::atomic<std::uint64_t>,32> words{};
};
template<std::size_t Capacity> struct RingStorage {
 AtomicHeader header{};
 std::array<AtomicSlot,Capacity> slots{};
};
static_assert(sizeof(Record)==256&&sizeof(Header)==128&&sizeof(AtomicHeader)==128&&sizeof(AtomicSlot)==264);
static_assert(offsetof(Header,capacity)==40&&offsetof(Header,build_id)==56&&offsetof(Header,published)==88);
static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
static_assert(sizeof(std::atomic<std::uint64_t>)==8&&std::atomic<std::uint64_t>::is_always_lock_free);
inline bool nonzero_build(const BuildId& id){for(auto b:id)if(b)return true;return false;}
template<std::size_t N> inline bool finite_values(const float (&values)[N]){
 for(float v:values)if(!std::isfinite(v))return false;
 return true;
}
inline Record sanitize(Record value){
 if(!finite_values(value.player))value.validity=(value.validity&~PlayerValid)|PlayerNonFinite;
 if(!finite_values(value.view))value.validity=(value.validity&~CameraValid)|CameraNonFinite;
 if(!finite_values(value.projection))value.validity=(value.validity&~ProjectionValid)|ProjectionNonFinite;
 value.reserved=0;value.reserved_tail[0]=value.reserved_tail[1]=0;
 return value;
}
// One producer, no allocation/SDK/filesystem calls. The public storage is the
// only remotely readable wire region. Never copy this object while publishing.
template<std::size_t Capacity=256> class ProducerRing {
 static_assert(Capacity==256||Capacity==1024,"Unsupported journal capacity");
 Header state_{};bool initialized_=false;
 void publish_header(bool clear_slots){
  const auto before=storage.header.sequence.fetch_add(1,std::memory_order_acq_rel);
  std::atomic_thread_fence(std::memory_order_release);
  if(clear_slots)for(auto& slot:storage.slots){slot.sequence.store(0,std::memory_order_relaxed);for(auto& word:slot.words)word.store(0,std::memory_order_relaxed);}
  std::uint64_t words[16];std::memcpy(words,&state_,sizeof(state_));
  for(unsigned i=0;i<15;++i)storage.header.words[i].store(words[i+1],std::memory_order_relaxed);
  storage.header.sequence.store(before+2,std::memory_order_release);state_.sequence=before+2;
 }
public:
 RingStorage<Capacity> storage{};
 bool initialize(std::uint64_t session,const BuildId& build){
  const auto version=storage.header.sequence.load(std::memory_order_relaxed);
  if(!session||!nonzero_build(build)||(initialized_&&session==state_.session)||(version&1)||version>UINT64_MAX-2)return false;
  Header h{};std::memcpy(h.marker,kJournalMarker,sizeof(h.marker));
  h.abi=kJournalAbi;h.header_size=sizeof(Header);h.record_size=sizeof(Record);h.slot_size=sizeof(AtomicSlot);
  h.capacity=Capacity;h.tick_hz=kTickHz;h.session=session;h.build_id=build;h.title_id=kTitleId;
  state_=h;publish_header(true);initialized_=true;return true;
 }
 bool capture(const Record& input){
  if(!initialized_||state_.published>=kMaxSequence||storage.header.sequence.load(std::memory_order_relaxed)>UINT64_MAX-2)return false;
  const auto serial=state_.published+1;auto& slot=storage.slots[(serial-1)%Capacity];
  const auto value=sanitize(input);std::uint64_t words[32];std::memcpy(words,&value,sizeof(value));
  slot.sequence.exchange(serial*2-1,std::memory_order_acq_rel);
  std::atomic_thread_fence(std::memory_order_release);
  for(unsigned i=0;i<32;++i)slot.words[i].store(words[i],std::memory_order_relaxed);
  slot.sequence.store(serial*2,std::memory_order_release);
  state_.published=serial;state_.overwrites=serial>Capacity?serial-Capacity:0;publish_header(false);return true;
 }
};
struct RemoteReader {
 void* context=nullptr;
 bool (*read)(void*,std::uint64_t,void*,std::size_t)=nullptr;
};
enum class ReadStatus {Record,Empty,Detached,Torn,Changed,Unsupported,Bounds,IoError};
struct Sample {Record record{};std::uint64_t sequence=0,missed=0,session=0;BuildId build_id{};};
inline bool valid_header(const Header& h){
 return h.sequence&&!(h.sequence&1)&&!std::memcmp(h.marker,kJournalMarker,16)&&h.abi==kJournalAbi
  &&h.header_size==sizeof(Header)&&h.record_size==sizeof(Record)&&h.slot_size==sizeof(AtomicSlot)
  &&(h.capacity==256||h.capacity==1024)&&h.tick_hz==kTickHz&&h.session&&nonzero_build(h.build_id)
  &&h.title_id==kTitleId&&!h.flags&&!h.reserved&&!h.reserved_tail&&h.published<=kMaxSequence
  &&h.overwrites==(h.published>h.capacity?h.published-h.capacity:0);
}
inline ReadStatus read_header(RemoteReader reader,std::uint64_t base,Header& out){
 if(!base||(base&7)||base>UINT64_MAX-sizeof(Header))return ReadStatus::Bounds;
 if(!reader.read)return ReadStatus::IoError;
 std::uint64_t before=0,after=0;Header value{};
 if(!reader.read(reader.context,base,&before,8))return ReadStatus::IoError;
 if(!before||(before&1))return ReadStatus::Torn;
 if(!reader.read(reader.context,base,&value,sizeof(value))||!reader.read(reader.context,base,&after,8))return ReadStatus::IoError;
 if(before!=after||value.sequence!=before)return ReadStatus::Torn;
 if(!valid_header(value))return ReadStatus::Unsupported;
 const auto size=std::uint64_t(sizeof(Header))+std::uint64_t(value.capacity)*sizeof(AtomicSlot);
 if(base>UINT64_MAX-size)return ReadStatus::Bounds;
 out=value;return ReadStatus::Empty;
}
class Cursor {
 Header header_{};std::uint64_t base_=0,next_=1,missed_=0,last_published_=0;bool attached_=false;
 bool same_identity(const Header& h)const{
  return h.session==header_.session&&h.build_id==header_.build_id&&h.capacity==header_.capacity&&h.title_id==header_.title_id;
 }
 ReadStatus changed(){attached_=false;return ReadStatus::Changed;}
public:
 ReadStatus attach(RemoteReader reader,std::uint64_t base,const BuildId& expected){
  attached_=false;Header h{};const auto status=read_header(reader,base,h);
  if(status!=ReadStatus::Empty)return status;
  if(!nonzero_build(expected)||h.build_id!=expected)return ReadStatus::Unsupported;
  header_=h;base_=base;next_=h.published>h.capacity?h.published-h.capacity+1:1;missed_=next_-1;last_published_=h.published;attached_=true;return ReadStatus::Empty;
 }
 ReadStatus read_next(RemoteReader reader,Sample& out){
  if(!attached_)return ReadStatus::Detached;
  Header current{};auto status=read_header(reader,base_,current);
  if(status!=ReadStatus::Empty)return status;
  if(!same_identity(current)||current.published<last_published_)return changed();
  const auto oldest=current.published>current.capacity?current.published-current.capacity+1:1;
  if(next_<oldest){missed_+=oldest-next_;next_=oldest;}
  header_=current;last_published_=current.published;
  if(next_>current.published)return ReadStatus::Empty;
  const auto address=base_+sizeof(Header)+((next_-1)%current.capacity)*sizeof(AtomicSlot);
  std::uint64_t before=0,after=0;Record record{};
  if(!reader.read||!reader.read(reader.context,address,&before,8))return ReadStatus::IoError;
  if(before!=next_*2)return ReadStatus::Torn;
  if(!reader.read(reader.context,address+8,&record,sizeof(record))||!reader.read(reader.context,address,&after,8))return ReadStatus::IoError;
  Header final{};status=read_header(reader,base_,final);
  if(status!=ReadStatus::Empty)return status;
  if(!same_identity(final)||final.published<last_published_)return changed();
  if(before!=after)return ReadStatus::Torn;
  Sample result{};result.record=sanitize(record);result.sequence=next_;result.missed=missed_;result.session=final.session;result.build_id=final.build_id;
  out=result;++next_;header_=final;last_published_=final.published;return ReadStatus::Record;
 }
 const Header& header()const{return header_;}
 std::uint64_t missed()const{return missed_;}
};
namespace detail {
class Csv {
 char* output_;std::size_t capacity_,size_=0;bool valid_;
public:
 Csv(char* out,std::size_t capacity):output_(out),capacity_(capacity),valid_(out&&capacity){if(valid_)output_[0]=0;}
 void text(const char* s){while(*s)character(*s++);}
 void character(char c){if(!valid_)return;if(size_+1>=capacity_){valid_=false;return;}output_[size_++]=c;}
 template<class T> void number(T value){char buffer[64];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),value);if(r.ec!=std::errc{}){valid_=false;return;}for(auto p=buffer;p!=r.ptr;++p)character(*p);}
 void floating(float value){char buffer[64];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),value,std::chars_format::general,std::numeric_limits<float>::max_digits10);if(r.ec!=std::errc{}){valid_=false;return;}for(auto p=buffer;p!=r.ptr;++p)character(*p);}
 void hex(std::uint64_t value,unsigned digits){constexpr char symbols[]="0123456789ABCDEF";for(unsigned i=digits;i;--i)character(symbols[(value>>((i-1)*4))&15]);}
 std::size_t finish(){if(!valid_){if(output_&&capacity_)output_[0]=0;return 0;}output_[size_]=0;return size_;}
};
}
inline std::size_t csv_header(const Header& h,char* output,std::size_t capacity){
 if(!valid_header(h)){if(output&&capacity)output[0]=0;return 0;}
 detail::Csv c(output,capacity);c.text("#re6_journal,abi=");c.number(h.abi);c.text(",session=");c.number(h.session);c.text(",title_id=");c.hex(h.title_id,16);c.text(",tick_hz=");c.number(h.tick_hz);c.text(",build_id=");for(auto b:h.build_id)c.hex(b,2);c.character('\n');
 c.text("session,sequence,dropped_total,frame,tick,sample_id,controller_id,controller_style,buttons,validity,actor,camera,lx,ly,rx,ry");
 for(const auto prefix:{"player_m","view_m","projection_m"})for(unsigned i=0;i<(prefix[0]=='p'&&prefix[1]=='r'?16u:12u);++i){c.character(',');c.text(prefix);c.number(i/4);c.number(i%4);}
 c.character('\n');return c.finish();
}
inline std::size_t csv_row(const Header& h,const Sample& sample,char* output,std::size_t capacity){
 if(!valid_header(h)||!sample.sequence||sample.sequence>kMaxSequence||sample.missed>=sample.sequence
    ||sample.session!=h.session||sample.build_id!=h.build_id){if(output&&capacity)output[0]=0;return 0;}
 const auto r=sanitize(sample.record);detail::Csv c(output,capacity);
 const bool input=(r.validity&InputValid)!=0;
 c.number(h.session);c.character(',');c.number(sample.sequence);c.character(',');c.number(sample.missed);c.character(',');c.number(r.frame);c.character(',');c.number(r.tick);c.character(',');if(input)c.number(r.sample_id);
 c.character(',');if(input)c.number(r.controller_id);c.character(',');if(input)c.number(r.controller_style);c.character(',');if(input)c.number(r.buttons);c.character(',');c.number(r.validity);c.character(',');c.number(r.actor);c.character(',');c.number(r.camera);
 for(auto value:r.sticks){c.character(',');if(input)c.number(value);}
 for(unsigned group=0;group<3;++group){const float* values=group==0?r.player:group==1?r.view:r.projection;const unsigned count=group==2?16:12,flag=group==0?PlayerValid:group==1?CameraValid:ProjectionValid;for(unsigned i=0;i<count;++i){c.character(',');if(r.validity&flag)c.floating(values[i]);}}
 c.character('\n');return c.finish();
}
}
