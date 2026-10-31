#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace re4 {
// Native338330 tests input(base124EC38)+19 bit3. The held word begins
// at +18 (profile::input_flags), so knife stance is bit11, not bit48.
inline constexpr std::uint64_t kKnifeHeldMask=0x800;
struct ModernMoveVec3 { float x=0,y=0,z=0; };
inline bool modern_move_finite(ModernMoveVec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}

// A reader must reject unmapped ranges. Parsing never writes game resource data.
struct ModernMoveMemory {
    void* context=nullptr;
    bool (*read)(void*,std::uintptr_t,void*,std::size_t)=nullptr;
    std::uintptr_t start=0;std::size_t size=0;
    bool contains(std::uintptr_t p,std::size_t n)const{
        return p>=start && p-start<=size && n<=size-(p-start);
    }
    bool copy(std::uintptr_t p,void* out,std::size_t n)const{
        if(!contains(p,n))return false;
        if(read)return read(context,p,out,n);
        std::memcpy(out,reinterpret_cast<const void*>(p),n);return true;
    }
    template<class T> bool value(std::uintptr_t p,T& v)const{return copy(p,&v,sizeof(v));}
    static ModernMoveMemory span(const void* p,std::size_t n){
        ModernMoveMemory r;r.start=reinterpret_cast<std::uintptr_t>(p);r.size=n;return r;
    }
};

struct ModernMoveSample {
    float frame=0,frames=0;std::uint32_t flags=2;
    std::uint8_t format=0,padding[3]{};
    std::uintptr_t data=0;
};
static_assert(sizeof(ModernMoveSample)==0x18);
using ModernMoveSampleFn=void (*)(const ModernMoveSample*,ModernMoveVec3*,std::uint16_t*,unsigned,unsigned);

// Switch MOTION_INFO. All writable fields and lookup arrays are private.
// Native entry points: sample 2B78F0, advance 2B8F10, root 2B8370, pose 2B8700.
class ModernMoveMotion {
public:
    ModernMoveMotion()=default;
    ModernMoveMotion(const ModernMoveMotion& other){*this=other;}
    ModernMoveMotion& operator=(const ModernMoveMotion& other){
        if(this!=&other){raw_=other.raw_;tracks_=other.tracks_;flags_=other.flags_;map_=other.map_;valid_=other.valid_;rebind();}return *this;
    }
    template<class T> T get(std::size_t o)const{T v{};if(o+sizeof(v)<=raw_.size())std::memcpy(&v,raw_.data()+o,sizeof(v));return v;}
    template<class T> void set(std::size_t o,T v){if(o+sizeof(v)<=raw_.size())std::memcpy(raw_.data()+o,&v,sizeof(v));}
    void* data(){return raw_.data();}
    const void* data()const{return raw_.data();}
    unsigned count()const{return get<std::uint8_t>(0x38);}
    unsigned frames()const{return get<std::uint16_t>(0xD8);}
    unsigned translation_track()const{return get<std::uint16_t>(0x50);}
    unsigned rotation_track()const{return get<std::uint16_t>(0x52);}
    const std::uint8_t* map()const{return map_.data();}
    const std::uintptr_t* tracks()const{return tracks_.data();}
    std::uintptr_t track(unsigned i)const{return i<count()?tracks_[i]:0;}
    bool valid()const{return valid_;}
    bool parse(const ModernMoveMemory& memory,std::uintptr_t resource,std::uintptr_t clip){
        valid_=false;raw_.fill(0);tracks_.fill(0);flags_.fill(0);map_.fill(255);
        std::uint16_t frame_word=0;std::uint8_t n=0;
        if(!memory.value(resource,frame_word)||!memory.value(resource+2,n)||!n)return false;
        const unsigned frame_count=frame_word&0x3FFF;
        if(frame_word!=frame_count||frame_count<2||frame_count>1023)return false;
        // Native 2B7290: alignDown(asset+3+3*channels+7,4).
        if(resource>std::numeric_limits<std::uintptr_t>::max()-10-3*unsigned(n))return false;
        const auto pointers=(resource+10+3*unsigned(n))&~std::uintptr_t(3);
        if(!memory.copy(resource+3,flags_.data(),2*n))return false;
        std::array<std::uint8_t,255> original_map{};
        if(!memory.copy(resource+3+2*n,original_map.data(),n))return false;
        unsigned root=0xFFFF,rotation=0xFFFF;
        for(unsigned i=0;i<n;++i){
            std::uint64_t value=0;
            if(!memory.value(pointers+8*i,value))return false;
            if(value<resource){if(value>std::numeric_limits<std::uintptr_t>::max()-resource)return false;value+=resource;}
            tracks_[i]=static_cast<std::uintptr_t>(value);
            if(!validate_track(memory,tracks_[i],flags_[i]>>12,frame_count))return false;
            const auto type=flags_[i]&255;
            if(type==1)root=i;
            if(type==0x40)rotation=i;
            // Root channels and all upper body stay outside the gait overlay.
            if(type!=1&&type!=0x40&&original_map[i]>=17&&original_map[i]<=25)map_[i]=original_map[i];
        }
        if(root==0xFFFF)return false;
        set<std::uintptr_t>(0,resource);set<float>(0x28,float(frame_word));
        set<std::uint8_t>(0x38,n);set<std::uint16_t>(0x50,std::uint16_t(root));
        set<std::uint16_t>(0x52,std::uint16_t(rotation));
        set<std::uint16_t>(0x54,5);set<std::uint32_t>(0x58,1u<<30);
        set<std::uint16_t>(0xD8,std::uint16_t(frame_count));set<float>(0xDC,1.f);
        if(clip){
            std::uint16_t logical=0;std::uint8_t mode=0;
            if(!memory.value(clip,logical)||logical<2||logical>1023||!memory.value(clip+2,mode))return false;
            std::uint16_t previous=0;
            for(unsigned i=0;i<logical;++i){
                std::uint16_t packed=0;
                if(!memory.value(clip+4+4*i,packed)||packed>frame_count*64||(!i&&packed!=0)||(i&&packed<=previous))return false;
                previous=packed;
            }
            set<std::uintptr_t>(0xC0,clip+4);set<std::uint16_t>(0xD8,logical);
            set<std::uint16_t>(0x54,std::uint16_t(5|((mode&1)?0x1000:0)));
        }
        valid_=true;rebind();return true;
    }
    bool prime(ModernMoveSampleFn sample){
        if(!valid_||!sample)return false;
        for(unsigned j=0;j<2;++j){
            const auto i=j?rotation_track():translation_track();if(i==0xFFFF)continue;
            ModernMoveSample request;request.frames=get<float>(0x28);request.format=std::uint8_t(flags_[i]>>12);request.data=tracks_[i];
            ModernMoveVec3 begin,end;std::uint16_t cursor[3]{};
            sample(&request,&begin,cursor,0,0);request.frame=request.frames;
            sample(&request,&end,cursor,0,0);
            ModernMoveVec3 cycle{end.x-begin.x,end.y-begin.y,end.z-begin.z};
            if(!modern_move_finite(begin)||!modern_move_finite(cycle)){valid_=false;return false;}
            set<ModernMoveVec3>(j?0x98:0x5C,begin);set<ModernMoveVec3>(j?0xA4:0x68,begin);
            set<ModernMoveVec3>(j?0xB0:0x74,cycle);
        }
        return true;
    }
private:
    void rebind(){if(!valid_)return;set<std::uintptr_t>(8,reinterpret_cast<std::uintptr_t>(tracks_.data()));set<std::uintptr_t>(0x40,reinterpret_cast<std::uintptr_t>(map_.data()));set<std::uintptr_t>(0x48,reinterpret_cast<std::uintptr_t>(flags_.data()));}
    static bool validate_track(const ModernMoveMemory& m,std::uintptr_t p,unsigned format,unsigned frames){
        constexpr unsigned strides[16]={12,8,6,0,10,6,4,0,9,5,3,0,0,0,0,4};
        if(format>=16||!strides[format])return false;
        for(unsigned axis=0;axis<3;++axis){
            std::uint16_t count=0;if(!m.value(p,count)||count>frames+1)return false;p+=2;
            if(!m.contains(p,std::size_t(count)*(2+strides[format])))return false;
            std::uint16_t previous=0;
            for(unsigned k=0;k<count;++k){std::uint16_t t=0;if(!m.value(p+2*k,t)||(k&&t<=previous)||t>frames)return false;previous=t;}
            p+=2*count;
            if(format==0||format==1||format==2||format==15){
                for(unsigned k=0;k<count;++k){float value=0;if(!m.value(p+k*strides[format],value)||!std::isfinite(value))return false;}
            }
            if(format==0){for(unsigned k=0;k<count;++k)for(unsigned c=1;c<3;++c){float v=0;if(!m.value(p+12*k+4*c,v)||!std::isfinite(v))return false;}}
            p+=std::size_t(count)*strides[format];
        }
        return true;
    }
    alignas(16) std::array<std::uint8_t,0x108> raw_{};
    std::array<std::uintptr_t,255> tracks_{};
    std::array<std::uint16_t,255> flags_{};
    std::array<std::uint8_t,255> map_{};
    bool valid_=false;
};

struct ModernMoveFrame {
    bool aim_enabled=false,knife_enabled=false,reload_enabled=false,active_player=false;
    bool generic_firearm=false,native_aim=false,input_live=false,scripted=false,frozen=false;
    unsigned character=0,mode=0,routine=0,phase=0,subphase=0;
    std::uint64_t held=0;
    float frame_factor=0,x=0,y=0;
};
inline bool modern_move_context_allowed(const ModernMoveFrame& f){
    if(!f.active_player||f.character!=0||f.mode||!f.input_live||f.scripted||f.frozen
       ||!std::isfinite(f.frame_factor)||f.frame_factor<=0||f.frame_factor>2
       ||!std::isfinite(f.x)||!std::isfinite(f.y))return false;
    return true;
}
// Proven generic31CC60 reload family, including initialization before the
// weapon's reload flag is written and playback after the aim button is released.
inline bool modern_reload_allowed(const ModernMoveFrame& f){
    return modern_move_context_allowed(f)&&f.reload_enabled&&f.generic_firearm
        &&f.routine==6&&f.phase==4&&f.subphase<=3;
}
inline bool modern_move_allowed(const ModernMoveFrame& f){
    if(!modern_move_context_allowed(f))return false;
    if(modern_reload_allowed(f))return true;
    if(f.routine==6)return f.aim_enabled&&f.generic_firearm&&f.native_aim&&(f.held&0x10)
        &&((f.phase==1&&f.subphase==1)||(f.phase==2&&f.subphase<=1));
    return f.routine==11&&f.knife_enabled&&(f.held&kKnifeHeldMask)&&f.subphase==1&&(f.phase==1||f.phase==2);
}
inline ModernMoveVec3 modern_move_disk(float x,float y){
    if(!std::isfinite(x)||!std::isfinite(y))return {};
    const double n=std::hypot(double(x),double(y));
    if(n>1){x=float(x/n);y=float(y/n);}return {x,0,y};
}
struct ModernMoveBasis {float m[3][3]={{1,0,0},{0,1,0},{0,0,1}};};
inline double modern_move_world_length(ModernMoveVec3 p,const ModernMoveBasis& b){
    if(!modern_move_finite(p))return -1;
    for(const auto& row:b.m)for(float v:row)if(!std::isfinite(v))return -1;
    const double x=double(b.m[0][0])*p.x+double(b.m[0][1])*p.y+double(b.m[0][2])*p.z;
    const double z=double(b.m[2][0])*p.x+double(b.m[2][1])*p.y+double(b.m[2][2])*p.z;
    return std::hypot(x,z);
}
inline bool modern_move_world_step(ModernMoveVec3 native,const ModernMoveBasis& b,float x,float y,float camera_yaw,ModernMoveVec3& out){
    out={};if(!std::isfinite(camera_yaw))return false;
    const auto disk=modern_move_disk(x,y);const double magnitude=std::hypot(double(disk.x),double(disk.z));
    const double length=modern_move_world_length(native,b);
    if(!(magnitude>0)||!(length>0)||!std::isfinite(length))return false;
    // Verified yaw convention: forward=(sin(yaw),cos(yaw)); screen right=(-cos,sin).
    const double s=std::sin(double(camera_yaw)),c=std::cos(double(camera_yaw));
    out={float((s*disk.z-c*disk.x)*length/magnitude),0,float((c*disk.z+s*disk.x)*length/magnitude)};
    return modern_move_finite(out);
}
struct ModernMoveEngine {
    ModernMoveSampleFn sample=nullptr;
    std::uint32_t (*advance)(void*)=nullptr;
    void (*root)(void*,void*,unsigned,ModernMoveVec3*,ModernMoveVec3*)=nullptr;
    void (*translate)(void*,const ModernMoveVec3*)=nullptr;
};
class ModernMoveCycle {
public:
    bool bind(const ModernMoveMotion& motion){valid_=motion.valid();if(valid_)motion_=motion;seen_=false;return valid_;}
    void reset(){valid_=false;seen_=false;}
    bool valid()const{return valid_;}
    ModernMoveMotion& motion(){return motion_;}
    bool step(const ModernMoveEngine& e,void* actor,std::uint64_t epoch,float x,float y,float camera_yaw,
              const ModernMoveBasis& basis,float gain,ModernMoveVec3& delta){
        delta={};if(seen_&&epoch==epoch_)return false;seen_=true;epoch_=epoch;
        if(!valid_||!e.advance||!e.root||!e.translate||!std::isfinite(gain))return false;
        const auto disk=modern_move_disk(x,y);const float magnitude=std::hypot(disk.x,disk.z);
        gain=std::min(1.f,std::max(0.f,gain));if(!(magnitude>0)||!gain)return false;
        auto reference=motion_;reference.set<float>(0xDC,1.f);motion_.set<float>(0xDC,gain*magnitude);
        ModernMoveVec3 full,full_rotation,candidate,rotation;
        e.advance(reference.data());e.root(actor,reference.data(),0,&full,&full_rotation);
        e.advance(motion_.data());e.root(actor,motion_.data(),0,&candidate,&rotation);
        const double full_length=modern_move_world_length(full,basis),candidate_length=modern_move_world_length(candidate,basis);
        if(!(full_length>=0)||!(candidate_length>=0)||!std::isfinite(full_length)||!std::isfinite(candidate_length)
           ||!modern_move_finite(full_rotation)||!modern_move_finite(rotation)){reset();return false;}
        const double cap=full_length*gain*magnitude;
        if(!modern_move_world_step(candidate,basis,disk.x,disk.z,camera_yaw,delta))return false;
        const double length=std::hypot(double(delta.x),double(delta.z));
        if(length>cap&&length>0){delta.x=float(delta.x*cap/length);delta.z=float(delta.z*cap/length);}
        if(!modern_move_finite(delta)||!(std::hypot(delta.x,delta.z)>0)){delta={};return false;}
        e.translate(actor,&delta);return true;
    }
private:
    ModernMoveMotion motion_;
    bool valid_=false,seen_=false;std::uint64_t epoch_=0;
};
struct ModernMoveIdentity {
    std::uintptr_t actor=0,skeleton=0,resources=0,motion_table=0;
    bool operator==(const ModernMoveIdentity& b)const{return actor==b.actor&&skeleton==b.skeleton&&resources==b.resources&&motion_table==b.motion_table;}
};
struct ModernMoveBone {float* local=nullptr;float* matrix=nullptr;std::uint32_t* flags=nullptr;std::uint16_t* cursors=nullptr;};

// 2B8700 uses live bone sample cursors even for a private motion. Isolate both
// native banks (1B4..1D7) so an aim key index cannot index a shorter walking
// channel, and gait sampling cannot contaminate the next native evaluation.
class ModernMoveCursorScope {
public:
    explicit ModernMoveCursorScope(const std::array<ModernMoveBone,9>& bones):bones_(bones){
        for(const auto& b:bones_)if(!b.cursors)return;
        for(unsigned i=0;i<9;++i){std::memcpy(saved_[i].data(),bones_[i].cursors,36);std::memset(bones_[i].cursors,0,36);}
        active_=true;
    }
    ModernMoveCursorScope(const ModernMoveCursorScope&)=delete;
    ModernMoveCursorScope& operator=(const ModernMoveCursorScope&)=delete;
    ~ModernMoveCursorScope(){restore();}
    bool valid()const{return active_;}
    void restore(){if(active_){for(unsigned i=0;i<9;++i)std::memcpy(bones_[i].cursors,saved_[i].data(),36);active_=false;}}
private:
    std::array<ModernMoveBone,9> bones_{};
    std::array<std::array<std::uint16_t,18>,9> saved_{};
    bool active_=false;
};
inline bool modern_move_leon_topology(const std::array<std::int16_t,26>& parents,unsigned count){
    // Independently checked in Switch pl00/pl0a/pl10 model bind tables.
    constexpr std::int16_t expected[26]={-1,0,1,2,3,2,5,5,7,8,9,2,11,11,13,14,15,0,17,18,19,20,17,22,23,24};
    if(count<26||count>=255)return false;
    for(unsigned i=0;i<26;++i)if(parents[i]!=expected[i])return false;
    return true;
}
inline float modern_move_lower_yaw(ModernMoveVec3 delta,float body_yaw,bool& backwards){
    backwards=false;if(!modern_move_finite(delta)||!std::isfinite(body_yaw)||!(std::hypot(delta.x,delta.z)>0))return 0;
    constexpr float pi=3.14159265358979323846f;
    float angle=std::remainder(std::atan2(delta.x,delta.z)-body_yaw,2*pi);
    if(std::fabs(angle)>pi*0.5f){backwards=true;angle=std::remainder(angle+pi,2*pi);}return angle;
}

// Restore only private overlay changes to local pose/matrices on the next
// update. World points, world matrices and previous collision positions are
// deliberately absent. Owner/range validation belongs to the caller.
class ModernMovePose {
public:
    bool pending()const{return pending_;}
    bool begin(ModernMoveIdentity identity,const std::array<ModernMoveBone,9>& bones){
        if(pending_)return false;
        for(const auto& b:bones){
            if(!b.local||!b.matrix||!b.flags)return false;
            for(unsigned i=0;i<9;++i)if(!std::isfinite(b.local[i]))return false;
            for(unsigned i=0;i<12;++i)if(!std::isfinite(b.matrix[i]))return false;
        }
        identity_=identity;bones_=bones;
        for(unsigned i=0;i<9;++i){std::memcpy(before_[i].local,bones[i].local,36);std::memcpy(before_[i].matrix,bones[i].matrix,48);before_[i].flags=*bones[i].flags;}
        pending_=true;return true;
    }
    bool finish(){
        if(!pending_)return false;
        for(unsigned i=0;i<9;++i){
            for(unsigned j=0;j<9;++j)if(!std::isfinite(bones_[i].local[j])){rollback();return false;}
            for(unsigned j=0;j<12;++j)if(!std::isfinite(bones_[i].matrix[j])){rollback();return false;}
        }
        for(unsigned i=0;i<9;++i){std::memcpy(written_[i].local,bones_[i].local,36);std::memcpy(written_[i].matrix,bones_[i].matrix,48);written_[i].flags=*bones_[i].flags;}
        finished_=true;return true;
    }
    bool restore(ModernMoveIdentity identity,const std::array<ModernMoveBone,9>& bones){
        if(!pending_)return false;
        if(!(identity==identity_)){discard();return false;}
        for(unsigned i=0;i<9;++i)if(bones[i].local!=bones_[i].local||bones[i].matrix!=bones_[i].matrix||bones[i].flags!=bones_[i].flags){discard();return false;}
        if(!finished_){rollback();return true;}
        for(unsigned i=0;i<9;++i){
            // A later native edit owns that field. Never replace it with a
            // stale snapshot merely because its actor address was reused.
            for(unsigned j=0;j<9;++j)if(same(bones[i].local[j],written_[i].local[j]))bones[i].local[j]=before_[i].local[j];
            if(!std::memcmp(bones[i].matrix,written_[i].matrix,48))std::memcpy(bones[i].matrix,before_[i].matrix,48);
            // 2B8700 owns bit16. Its bit28 is shared with native IK
            // (2935A8/2935F0/293648) and must never be rolled back here.
            const auto mask=(before_[i].flags^written_[i].flags)&0x00010000u;
            if((*bones[i].flags&mask)==(written_[i].flags&mask))*bones[i].flags=(*bones[i].flags&~mask)|(before_[i].flags&mask);
        }
        discard();return true;
    }
    void discard(){pending_=false;finished_=false;}
    void rollback(){
        if(!pending_)return;
        for(unsigned i=0;i<9;++i){std::memcpy(bones_[i].local,before_[i].local,36);std::memcpy(bones_[i].matrix,before_[i].matrix,48);*bones_[i].flags=before_[i].flags;}
        discard();
    }
private:
    static bool same(float a,float b){return !std::memcmp(&a,&b,4);}
    struct Saved {float local[9]{},matrix[12]{};std::uint32_t flags=0;};
    std::array<Saved,9> before_{},written_{};
    std::array<ModernMoveBone,9> bones_{};
    ModernMoveIdentity identity_{};bool pending_=false,finished_=false;
};
}
