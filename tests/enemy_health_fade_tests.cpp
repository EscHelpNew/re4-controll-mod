#include "re4/enemy_health_fade.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr,"enemy health fade line %d: %s\n",__LINE__,#c); std::exit(1); } } while (false)
namespace {
using namespace re4;
constexpr std::uint64_t half=2880000, duration=5760000, quarter=1440000;
EnemyHealthFadeTarget target(std::uint64_t id=1) { return {id,{10,20,30},75,100}; }
const EnemyHealthFadeEntry* find(const EnemyHealthFade& fade,std::uint64_t id) {
    for (const auto& entry:fade.entries()) if (entry.active&&entry.target.id==id) return &entry;
    return nullptr;
}
void opacity(const EnemyHealthFade& fade,std::uint64_t id,float expected) {
    const auto* entry=find(fade,id); CHECK(entry); CHECK(std::fabs(entry->opacity-expected)<.00001f);
}
unsigned count(const EnemyHealthFade& fade) {
    unsigned result=0;
    for (const auto& entry:fade.entries()) {
        if (entry.active) ++result;
        CHECK(std::isfinite(entry.opacity)&&entry.opacity>=0&&entry.opacity<=1);
    }
    return result;
}
void fades_in_and_out_over_300ms() {
    EnemyHealthFade fade; const auto a=target();
    fade.update(0,&a); opacity(fade,1,0);
    fade.update(quarter,&a); opacity(fade,1,.15625f);
    fade.update(half,&a); opacity(fade,1,.5f);
    fade.update(duration,&a); opacity(fade,1,1);
    fade.update(duration,nullptr); opacity(fade,1,1);
    fade.update(duration+half,nullptr); opacity(fade,1,.5f);
    fade.update(duration*2,nullptr); CHECK(count(fade)==0);
}
void updates_data_without_restarting_fade_or_borrowing_storage() {
    EnemyHealthFade fade; auto a=target();
    fade.update(100,&a); a.current=40; a.anchor={11,22,33};
    fade.update(100+half,&a); opacity(fade,1,.5f);
    CHECK(find(fade,1)->target.current==40&&find(fade,1)->target.anchor.y==22);
    a.current=1; a.anchor.y=999;
    CHECK(find(fade,1)->target.current==40&&find(fade,1)->target.anchor.y==22);
    fade.update(100+duration,nullptr); opacity(fade,1,1);
    CHECK(find(fade,1)->target.current==40&&find(fade,1)->target.anchor.y==22);
    fade.update(100+duration+half,nullptr); opacity(fade,1,.5f);
}
void switches_targets_and_reverses_from_current_opacity() {
    EnemyHealthFade fade; const auto a=target(),b=target(2);
    fade.update(0,&a); fade.update(duration,&a);
    fade.update(duration,&b); opacity(fade,1,1); opacity(fade,2,0);
    fade.update(duration+half,&b); opacity(fade,1,.5f); opacity(fade,2,.5f);
    fade.update(duration+half,&a); opacity(fade,1,.5f); opacity(fade,2,.5f);
    fade.update(duration*2,&a); opacity(fade,1,.75f); opacity(fade,2,.25f);
    fade.update(duration*2+half,&a); opacity(fade,1,1); CHECK(!find(fade,2));
    fade.reset(); fade.update(0,&a); fade.update(half,nullptr); opacity(fade,1,.5f);
    fade.update(duration,nullptr); opacity(fade,1,.25f);
    fade.update(duration,&a); opacity(fade,1,.25f);
    fade.update(duration+half,&a); opacity(fade,1,.625f);
    fade.update(duration*2,&a); opacity(fade,1,1);
}
void repeated_updates_do_not_advance_or_restart_time() {
    EnemyHealthFade fade; const auto a=target();
    fade.update(0,&a); fade.update(half,&a);
    for (unsigned i=0;i<1000;++i) fade.update(half,&a);
    opacity(fade,1,.5f); fade.update(duration,&a); opacity(fade,1,1);
    fade.update(duration,nullptr); fade.update(duration+half,nullptr);
    for (unsigned i=0;i<1000;++i) fade.update(duration+half,nullptr);
    opacity(fade,1,.5f); fade.update(duration*2,nullptr); CHECK(count(fade)==0);
}
void clock_rollback_starts_fresh_and_reset_clears_everything() {
    EnemyHealthFade fade; const auto a=target(),b=target(2);
    fade.update(100,&a); fade.update(100+duration,&a);
    fade.update(50,&b); CHECK(!find(fade,1)); opacity(fade,2,0);
    fade.update(50+half,&b); opacity(fade,2,.5f);
    fade.update(1,nullptr); CHECK(count(fade)==0);
    fade.update(10,&a); fade.update(10+duration,&a); fade.reset(); CHECK(count(fade)==0);
    fade.update(0,&a); opacity(fade,1,0); fade.update(half,&a); opacity(fade,1,.5f);
}
void capacity_evicts_least_opaque_old_target() {
    EnemyHealthFade fade; auto a=target();
    fade.update(0,&a); fade.update(duration,&a);
    auto b=target(2); fade.update(duration,&b); fade.update(duration+quarter,&b);
    auto c=target(3); fade.update(duration+quarter,&c); fade.update(duration+half,&c);
    auto d=target(4); fade.update(duration+half,&d); fade.update(duration+quarter*3,&d);
    CHECK(count(fade)==4);
    CHECK(find(fade,2)->opacity<find(fade,1)->opacity);
    CHECK(find(fade,2)->opacity<find(fade,3)->opacity);
    CHECK(find(fade,2)->opacity<find(fade,4)->opacity);
    const float old_selected=find(fade,4)->opacity;
    auto e=target(5); fade.update(duration+quarter*3,&e);
    CHECK(count(fade)==4&&!find(fade,2)); opacity(fade,5,0); opacity(fade,4,old_selected);
    for (unsigned i=0;i<100;++i) {
        e.current=60; fade.update(duration+quarter*3,&e);
        CHECK(count(fade)==4&&find(fade,5)->target.current==60);
    }
    fade.update(duration*3,&e); CHECK(count(fade)==1); opacity(fade,5,1);
}
void invalid_targets_fade_out_without_copying_bad_data() {
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for (unsigned i=0;i<11;++i) {
        EnemyHealthFade fade; const auto a=target(); fade.update(0,&a); fade.update(duration,&a);
        auto bad=a;
        switch (i) {
        case 0: bad.id=0; break;
        case 1: bad.current=0; break;
        case 2: bad.current=-1; break;
        case 3: bad.current=32768; break;
        case 4: bad.maximum=0; break;
        case 5: bad.maximum=-1; break;
        case 6: bad.maximum=32768; break;
        case 7: bad.anchor.x=nan; break;
        case 8: bad.anchor.y=inf; break;
        case 9: bad.anchor.z=-inf; break;
        default: bad.anchor.z=nan; break;
        }
        fade.update(duration,&bad); opacity(fade,1,1);
        CHECK(find(fade,1)->target.current==75&&find(fade,1)->target.anchor.z==30);
        fade.update(duration+half,&bad); opacity(fade,1,.5f);
        fade.update(duration*2,&bad); CHECK(count(fade)==0);
        fade.update(duration*3,&bad); CHECK(count(fade)==0);
    }
    EnemyHealthFade fade; auto overhealed=target(); overhealed.current=150;
    fade.update(0,&overhealed); fade.update(duration,&overhealed); opacity(fade,1,1);
}
}
int main() {
    fades_in_and_out_over_300ms();
    updates_data_without_restarting_fade_or_borrowing_storage();
    switches_targets_and_reverses_from_current_opacity();
    repeated_updates_do_not_advance_or_restart_time();
    clock_rollback_starts_fresh_and_reset_clears_everything();
    capacity_evicts_least_opaque_old_target();
    invalid_targets_fade_out_without_copying_bad_data();
    std::puts("enemy health fade: timing, reversal, snapshots, bounded slots, invalid targets PASS");
}
