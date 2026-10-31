#pragma once
#include "journal.hpp"

namespace re6 {
// A sink finalizes only on finish(true). Failed recordings retain their .part
// file for recovery; the caller must never expose them as completed captures.
struct RecorderSink {
 void* context=nullptr;
 bool (*begin)(void*,const Header&)=nullptr;
 bool (*write)(void*,const char*,std::size_t)=nullptr;
 bool (*finish)(void*,bool)=nullptr;
};
enum class RecorderState {Idle,Recording,Stopping,Saved,Failed};
enum class RecorderError {None,Attach,Open,Write,Finalize,Read,Changed,Stalled};
class Recorder {
 Cursor cursor_{};RemoteReader reader_{};RecorderSink sink_{};
 Header identity_{};std::uint64_t base_=0,consumed_=0,stop_=0,records_=0,dropped_=0;
 RecorderState state_=RecorderState::Idle;RecorderError error_=RecorderError::None;
 unsigned stalled_=0;bool opened_=false;
 char csv_[kCsvBufferBytes]{};
 bool same_identity(const Header& h)const{return h.session==identity_.session&&h.title_id==identity_.title_id&&h.build_id==identity_.build_id&&h.capacity==identity_.capacity;}
 void save(){
  const bool ok=sink_.finish&&sink_.finish(sink_.context,true);opened_=false;
  state_=ok?RecorderState::Saved:RecorderState::Failed;error_=ok?RecorderError::None:RecorderError::Finalize;
 }
public:
 bool start(RemoteReader reader,std::uint64_t base,const BuildId& build,std::uint64_t title,RecorderSink sink){
  if(active())return false;
  reader_=reader;sink_=sink;base_=base;records_=dropped_=stop_=0;stalled_=0;error_=RecorderError::None;opened_=false;
  if(cursor_.attach(reader,base,build,title,true)!=ReadStatus::Empty){state_=RecorderState::Failed;error_=RecorderError::Attach;return false;}
  identity_=cursor_.header();consumed_=identity_.published;
  if(!sink_.begin||!sink_.begin(sink_.context,identity_)){state_=RecorderState::Failed;error_=RecorderError::Open;return false;}
  opened_=true;state_=RecorderState::Recording;
  const auto bytes=csv_header(identity_,csv_,sizeof(csv_));
  if(!bytes||!sink_.write||!sink_.write(sink_.context,csv_,bytes)){abort(RecorderError::Write);return false;}
  detail::Csv metadata(csv_,sizeof(csv_));metadata.text("#capture,start_after_sequence=");metadata.number(identity_.published);
  metadata.text(",capacity=");metadata.number(identity_.capacity);metadata.character('\n');
  const auto metadata_bytes=metadata.finish();
  if(!metadata_bytes||!sink_.write(sink_.context,csv_,metadata_bytes)){abort(RecorderError::Write);return false;}
  return true;
 }
 bool request_stop(){
  if(state_==RecorderState::Stopping)return true;
  if(state_!=RecorderState::Recording)return false;
  Header h{};const auto status=read_header(reader_,base_,h);
  if(status==ReadStatus::Torn)return false;
  if(status!=ReadStatus::Empty){abort(RecorderError::Read);return false;}
  if(!same_identity(h)||h.published<consumed_){abort(RecorderError::Changed);return false;}
  stop_=h.published;state_=RecorderState::Stopping;return true;
 }
 // Each call is bounded; the worker supplies scheduling and retry delays.
 void poll(unsigned budget=64){
  if(!active())return;
  for(unsigned i=0;i<budget;++i){
   if(state_==RecorderState::Stopping&&consumed_>=stop_){save();return;}
   Sample sample{};const auto status=cursor_.read_next(reader_,sample);
   if(status==ReadStatus::Torn){if(++stalled_>=250)abort(RecorderError::Stalled);return;}
   if(status==ReadStatus::Empty){stalled_=0;return;}
   if(status!=ReadStatus::Record){abort(status==ReadStatus::Changed||status==ReadStatus::Detached?RecorderError::Changed:RecorderError::Read);return;}
   stalled_=0;
   if(state_==RecorderState::Stopping&&sample.sequence>stop_){dropped_+=stop_-consumed_;consumed_=stop_;save();return;}
   const auto bytes=csv_row(identity_,sample,csv_,sizeof(csv_));
   if(!bytes||!sink_.write||!sink_.write(sink_.context,csv_,bytes)){abort(RecorderError::Write);return;}
   dropped_+=sample.sequence-consumed_-1;consumed_=sample.sequence;++records_;
  }
  if(state_==RecorderState::Stopping&&consumed_>=stop_)save();
 }
 void abort(RecorderError error){if(opened_&&sink_.finish)sink_.finish(sink_.context,false);opened_=false;state_=RecorderState::Failed;error_=error;}
 bool active()const{return state_==RecorderState::Recording||state_==RecorderState::Stopping;}
 RecorderState state()const{return state_;}
 RecorderError error()const{return error_;}
 std::uint64_t records()const{return records_;}
 std::uint64_t dropped()const{return dropped_;}
};
}
