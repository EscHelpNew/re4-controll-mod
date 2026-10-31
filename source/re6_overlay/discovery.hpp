#pragma once
#include "re6/journal.hpp"
#include <algorithm>

namespace telemetry {
// Limit covers all readable mutable-code mappings, with a hard upper bound
// against corrupt metadata. Scan every page, including module BSS. Overlapping
// reads allow the complete header to straddle any page boundary.
inline constexpr std::uint64_t kScanLimit=512ULL*1024*1024;
inline std::uint64_t find_journal(re6::RemoteReader reader,std::uint64_t base,std::uint64_t size,
 const re6::BuildId& build,std::uint64_t title,std::uint64_t& examined,
 bool (*cancelled)(void*)=nullptr,void* context=nullptr){
 if(!base||(base&7)||size<sizeof(re6::Header)||base>UINT64_MAX-size||!reader.read)return 0;
 unsigned char buffer[0x1000+sizeof(re6::Header)];
 for(std::uint64_t offset=0;offset<size;offset+=0x1000){
  if((cancelled&&cancelled(context))||examined>=kScanLimit)return 0;
  const auto count=std::size_t(std::min<std::uint64_t>(sizeof(buffer),size-offset));
  const auto page=std::min<std::uint64_t>(0x1000,size-offset);
  if(page>kScanLimit-examined)return 0;
  examined+=page;
  if(!reader.read(reader.context,base+offset,buffer,count))continue;
  for(std::size_t j=0;j<page&&j+sizeof(re6::Header)<=count;j+=8){
   if(std::memcmp(buffer+j+8,re6::kJournalMarker,16))continue;
   re6::Header h{};
   if(re6::read_header(reader,base+offset+j,h)!=re6::ReadStatus::Empty||h.build_id!=build||h.title_id!=title)continue;
   const auto ring_size=sizeof(h)+std::uint64_t(h.capacity)*sizeof(re6::AtomicSlot);
   if(ring_size<=size-offset-j)return base+offset+j;
  }
 }
 return 0;
}
}
