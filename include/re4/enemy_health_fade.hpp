#pragma once
#include "re4/view.hpp"
#include <array>
#include <cmath>
#include <cstdint>

namespace re4 {
struct EnemyHealthFadeTarget {
    std::uint64_t id=0;
    ViewVec3 anchor{};
    int current=0,maximum=0;
};
struct EnemyHealthFadeEntry {
    EnemyHealthFadeTarget target{};
    float opacity=0;
    bool active=false;
};
class EnemyHealthFade {
public:
    static constexpr std::uint64_t kDurationTicks=5760000;
    void reset() {
        entries_={}; transitions_={}; last_tick_=0; has_tick_=false;
    }
    void update(std::uint64_t now,const EnemyHealthFadeTarget* visible) {
        if (has_tick_&&now<last_tick_) reset();
        last_tick_=now; has_tick_=true;
        const std::uint64_t selected=valid(visible)?visible->id:0;
        for (unsigned i=0;i<entries_.size();++i) {
            auto& entry=entries_[i];
            if (!entry.active) continue;
            auto& transition=transitions_[i];
            const auto elapsed=now-transition.start;
            if (elapsed>=kDurationTicks) entry.opacity=transition.goal;
            else {
                const float t=float(elapsed)/float(kDurationTicks);
                const float smooth=t*t*(3.f-2.f*t);
                entry.opacity=transition.from+(transition.goal-transition.from)*smooth;
            }
            const float goal=entry.target.id==selected?1.f:0.f;
            if (transition.goal!=goal) transition={now,entry.opacity,goal};
            if (goal==0&&entry.opacity==0) { entry={}; transition={}; }
        }
        if (!selected) return;
        unsigned slot=entries_.size();
        for (unsigned i=0;i<entries_.size();++i) {
            if (entries_[i].active&&entries_[i].target.id==selected) {
                entries_[i].target=*visible;
                return;
            }
            if (!entries_[i].active&&slot==entries_.size()) slot=i;
        }
        if (slot==entries_.size()) {
            slot=0;
            for (unsigned i=1;i<entries_.size();++i)
                if (entries_[i].opacity<entries_[slot].opacity) slot=i;
        }
        entries_[slot]={*visible,0,true};
        transitions_[slot]={now,0,1};
    }
    const std::array<EnemyHealthFadeEntry,4>& entries() const { return entries_; }
private:
    struct Transition {
        std::uint64_t start=0;
        float from=0,goal=0;
    };
    static bool valid(const EnemyHealthFadeTarget* target) {
        return target&&target->id&&target->current>0&&target->current<=32767
            &&target->maximum>0&&target->maximum<=32767
            &&std::isfinite(target->anchor.x)&&std::isfinite(target->anchor.y)
            &&std::isfinite(target->anchor.z);
    }
    std::array<EnemyHealthFadeEntry,4> entries_{};
    std::array<Transition,4> transitions_{};
    std::uint64_t last_tick_=0;
    bool has_tick_=false;
};
}
