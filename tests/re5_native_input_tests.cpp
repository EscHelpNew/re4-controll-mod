#include "re5/native_input.hpp"
#include <map>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 native input line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
struct Memory {
 std::map<std::uintptr_t,std::vector<std::uint8_t>> regions;
 void add(std::uintptr_t at,std::size_t size){regions.emplace(at,std::vector<std::uint8_t>(size));}
 static bool range(void* ctx,std::uintptr_t at,std::size_t size){for(const auto& [base,b]:static_cast<Memory*>(ctx)->regions)if(at>=base&&at-base<=b.size()&&size<=b.size()-(at-base))return true;return false;}
 static bool read(void* ctx,std::uintptr_t at,void* out,std::size_t size){for(const auto& [base,b]:static_cast<Memory*>(ctx)->regions)if(at>=base&&at-base<=b.size()&&size<=b.size()-(at-base)){std::memcpy(out,b.data()+at-base,size);return true;}return false;}
 template<class T>void put(std::uintptr_t at,T value){for(auto& [base,b]:regions)if(at>=base&&at-base+sizeof(T)<=b.size()){std::memcpy(b.data()+at-base,&value,sizeof(value));return;}assert(false);}
};
}
int main(){
 constexpr std::uintptr_t main=0x10000000,players=0x20000000,pads=0x30000000,a=0x40000000,b=0x40010000,wa=0x50000000,wb=0x50010000;
 Memory m;m.add(main+0x1CF8CC8,8);m.add(main+0x26A9288,8);m.add(players,0x100);m.add(pads,0x9000);
 for(auto actor:{a,b})m.add(actor,0x4170);
 for(auto wrapper:{wa,wb})m.add(wrapper,0x900);
 m.put(main+0x1CF8CC8,players);m.put(main+0x26A9288,pads);
 // Player1 may occupy actor slot2, with different native default stick/motion pads.
 m.put(a+0x3F2C,std::uint8_t(2));m.put(a+0x3F49,std::uint8_t(0));m.put(a+0x3F40,std::uint32_t(0));
 m.put(players+0x40,a);m.put(a+0x3EE8,wa);m.put(wa+0x48,a);m.put(wa+0x34C,std::uint32_t(0));
 m.put(pads+0x8C98,std::uint32_t(8));m.put(pads+0x8C94,std::uint32_t(3));
 m.put(pads+8*0xEE0+0x670,std::uint32_t(32));m.put(pads+8*0xEE0+0x674,std::uint32_t(0x10));
 m.put(pads+3*0xEE0+0x670,std::uint32_t(3));m.put(pads+3*0xEE0+0x674,std::uint32_t(1));
 // Player2 uses logical selector1, not its actor slot1 as an assumed input source.
 m.put(b+0x3F2C,std::uint8_t(1));m.put(b+0x3F49,std::uint8_t(1));m.put(b+0x3F40,std::uint32_t(0));
 m.put(players+0x38,b);m.put(b+0x3EE8,wb);m.put(wb+0x48,b);m.put(wb+0x34C,std::uint32_t(0));
 m.put(pads+0xEE0+0x670,std::uint32_t(1));m.put(pads+0xEE0+0x674,std::uint32_t(2));
 const re5::MemoryReader reader{&m,Memory::read,Memory::range};re5::ResolvedPlayer first,second;
 assert(re5::resolve_player(reader,main,a,wa+0x350,first));
 assert(first.actor==a&&first.local_player==0&&first.actor_slot==2&&first.sticks.controller==32&&first.sticks.style==2&&first.motion.controller==3&&first.motion.style==1);
 assert(re5::resolve_player(reader,main,b,wb+0x350,second));
 assert(second.local_player==1&&second.motion.controller==1&&second.sticks.controller==1&&second.motion.style==4);
 // Native engine styles differ from the SDK: full1, dual2, handheld16.
 for(const auto& [native,sdk]:{std::pair{1u,1u},std::pair{2u,4u},std::pair{16u,2u}}){
  m.put(pads+0xEE0+0x674,native);assert(re5::resolve_player(reader,main,b,wb+0x350,second));assert(second.sticks.style==sdk&&second.motion.style==sdk);
 }
 for(unsigned unsupported:{0u,4u,8u,32u,64u}){m.put(pads+0xEE0+0x674,unsupported);assert(!re5::resolve_player(reader,main,b,wb+0x350,second));}
 m.put(pads+0xEE0+0x674,std::uint32_t(2));assert(re5::resolve_player(reader,main,b,wb+0x350,second));
 assert(!re5::resolve_player(reader,main,a,wb+0x350,second)&&second.local_player==1);
 for(std::uint32_t owner:{1u,2u,255u}){m.put(a+0x3F40,owner);assert(!re5::resolve_player(reader,main,a,wa+0x350,second));}m.put(a+0x3F40,std::uint32_t(0));
 m.put(wa+0x48,b);assert(!re5::resolve_player(reader,main,a,wa+0x350,second));m.put(wa+0x48,a);
 m.put(wa+0x34C,std::uint32_t(1));assert(!re5::resolve_player(reader,main,a,wa+0x350,second));m.put(wa+0x34C,std::uint32_t(0));
 m.put(players+0x40,b);assert(!re5::resolve_player(reader,main,a,wa+0x350,second));m.put(players+0x40,a);
 m.put(a+0x3F2C,std::uint8_t(4));assert(!re5::resolve_player(reader,main,a,wa+0x350,second));m.put(a+0x3F2C,std::uint8_t(2));
 m.put(a+0x3F49,std::uint8_t(2));assert(!re5::resolve_player(reader,main,a,wa+0x350,second));m.put(a+0x3F49,std::uint8_t(0));
 m.put(pads+0x8C94,std::uint32_t(9));assert(!re5::resolve_player(reader,main,a,wa+0x350,second));m.put(pads+0x8C94,std::uint32_t(3));
 m.put(pads+3*0xEE0+0x670,std::uint32_t(0));assert(!re5::resolve_player(reader,main,a,wa+0x350,second));
 assert(!re5::resolve_player(reader,main,0,wa+0x350,second));
 std::puts("RE5 native ownership: local human, actor/member/pad identity and independent motion/stick assignments PASS");
}
