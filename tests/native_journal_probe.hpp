#pragma once
// Keep the real ring implementation, observing the module's use of its boundary.
#define ProducerRing UnobservedProducerRing
#include "re6/journal.hpp"
#undef ProducerRing
namespace native_journal_test {
inline unsigned storage_instances=0,initializations=0,captures=0;
}
namespace re6 {
template<std::size_t Capacity=256> class ProducerRing : public UnobservedProducerRing<Capacity> {
public:
 ProducerRing(){++native_journal_test::storage_instances;}
 bool initialize(std::uint64_t session,const BuildId& build,std::uint64_t title=kTitleId){
  ++native_journal_test::initializations;
  return UnobservedProducerRing<Capacity>::initialize(session,build,title);
 }
 bool capture(const Record& input){
  ++native_journal_test::captures;
  return UnobservedProducerRing<Capacity>::capture(input);
 }
};
}
