#if !__has_include("re5/frame_snapshot.hpp")
#include <cstdio>
int main(){std::fputs("FAIL: coherent frame snapshot is unavailable\n",stderr);return 1;}
#else
#include "re5/frame_snapshot.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <future>
#define check(c) do{if(!(c)){std::fprintf(stderr,"frame snapshot line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
struct Frame {std::uint64_t serial=0,actor=0,command=0,generation=0,pad=0,settings=0,axes=0,tick=0;};
Frame make_frame(std::uint64_t n){return {n,n+1,n+2,n+3,n+4,n+5,n+6,n+7};}
void coherent(const Frame& f){check(f.actor==f.serial+1&&f.command==f.serial+2&&f.generation==f.serial+3&&f.pad==f.serial+4&&f.settings==f.serial+5&&f.axes==f.serial+6&&f.tick==f.serial+7);}
int main(){
 re5::FrameSnapshot<Frame> slots[2];Frame out=make_frame(99);
 check(!slots[0].read(out)&&out.serial==99);
 check(slots[0].publish(make_frame(1))&&slots[1].publish(make_frame(100)));
 check(slots[0].read(out)&&out.serial==1);coherent(out);
 check(slots[1].read(out)&&out.serial==100);coherent(out);
 // A held slot returns immediately: neither callback may spin or wait for it.
 slots[0].lock.test_and_set();check(!slots[0].publish(make_frame(2)));check(!slots[0].read(out));
 check(slots[1].read(out)&&out.serial==100);slots[0].lock.clear();
 check(slots[0].read(out)&&out.serial==1);
 std::atomic<bool> done{false};std::atomic<unsigned> successes{0};std::promise<void> ready;auto started=ready.get_future();
 std::thread reader([&]{Frame f;check(slots[0].read(f));coherent(f);++successes;ready.set_value();while(!done.load(std::memory_order_acquire)){if(slots[0].read(f)){coherent(f);++successes;}}});
 started.wait();
 std::thread writer([&]{for(std::uint64_t n=2;n<150000;++n)slots[0].publish(make_frame(n));done.store(true,std::memory_order_release);});
 writer.join();reader.join();check(successes.load()>0);
 check(slots[0].publish(make_frame(150000))&&slots[0].read(out)&&out.serial==150000);coherent(out);
 check(slots[1].read(out)&&out.serial==100);coherent(out);
 std::puts("RE5 frame snapshot: coherent concurrent copies, isolated players and nonblocking contention PASS");
}
#endif
