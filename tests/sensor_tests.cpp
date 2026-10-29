#include "re4/sensor.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
struct Backend {
    static inline unsigned gets=0, starts=0, stops=0, reads=0;
    static inline unsigned get_rc=0, start_rc=0, read_rc=0, last=0;
    static inline int count=1, samples=1;
    static unsigned handles(unsigned* out, int* size, int, unsigned id, unsigned style) {
        ++gets; assert(id == 0 || id == 32); assert(style == 1 || style == 2 || style == 4);
        out[0]=10; out[1]=20; *size=count; return get_rc;
    }
    static unsigned start(const unsigned& handle) { ++starts; last=handle; return start_rc; }
    static unsigned stop(const unsigned& handle) { ++stops; last=handle; return 0; }
    static unsigned read(int* n, re4::SensorState* out, int, const unsigned& handle) {
        ++reads; last=handle; *n=samples; out[0].number=42; return read_rc;
    }
};
int main() {
    using Reader=re4::SensorReader<Backend>;
    Reader reader;
    re4::SensorState states[16]{};
    assert(reader.read(states, 16) == 0 && Backend::reads == 0);
    assert(reader.select(32,2,1,true) && reader.running());
    assert(Backend::gets == 1 && Backend::starts == 1);
    assert(!reader.select(32,2,1,true) && Backend::starts == 1);
    assert(reader.read(states,16) == 1 && states[0].number == 42);
    Backend::read_rc=3;
    assert(reader.read(states,16) == 0 && reader.result == 3);
    assert(!reader.running() && Backend::stops == 1);
    const auto failed_reads = Backend::reads;
    assert(reader.read(states,16) == 0 && Backend::reads == failed_reads);
    Backend::read_rc=0; Backend::count=2;
    assert(reader.select(0,4,1|16,true)); // SDK bit4 is RightConnected; bit3 is LeftWired.
    assert(Backend::stops == 1 && Backend::last == 20); // Prefer connected right Joy-Con.
    assert(reader.select(0,4,1|16,false) && !reader.running());
    assert(Backend::stops == 2);
    Backend::start_rc=5;
    assert(reader.select(0,1,1,true) && !reader.running() && reader.result == 5);
    const auto calls=Backend::starts;
    assert(!reader.select(0,1,1,true) && Backend::starts == calls); // No IPC retry storm.
    Backend::get_rc=7;
    assert(reader.select(32,2,1,true) && !reader.running() && reader.result == 7);
    assert(Backend::starts == calls);
    reader.stop(); assert(Backend::stops == 2);
    Backend::get_rc=0; Backend::start_rc=0; Backend::samples=17;
    assert(reader.select(0,1,1,true) && reader.running());
    assert(reader.read(states,16) == 0 && !reader.running()); // Malformed count cannot index stack.
    assert(reader.result == 0xFFFFFFFFu && Backend::stops == 3);
    Backend::samples=1;Backend::start_rc=5;Backend::count=1;
    assert(!reader.restart() && !reader.running() && reader.result==5);
    Backend::start_rc=0;assert(reader.restart() && reader.running());reader.stop();
    Backend::count=2;
    for(unsigned attributes:{1u|4u,1u|16u,1u|4u|16u,1u|4u|8u}){
        reader.select(0,4,attributes,true);
        assert(Backend::last==((attributes&16)?20u:10u));
    }
    reader.select(0,4,0,true);assert(!reader.running());
    std::puts("sensor lifecycle: selection, failures, bounds and reconnect PASS");
}
