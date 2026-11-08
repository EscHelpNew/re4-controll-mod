#include "../source/overlay/game_link.cpp"
#include <cassert>
#include <limits>
#include <vector>
namespace {
constexpr u64 software=0xA00000,slot_offset=0x1184150;
unsigned slot_reads=0,write_count=0;
bool switch_slot=false,switch_id=false,fail_final_id=false;
unsigned fail_slot_read=0;
u64 main_base=0,main_size=0,fail_read_address=0;
struct Segment {u64 address;std::vector<u8> bytes;};
std::vector<Segment> segments;
template<class T>void segment(u64 at,const T& value){Segment s{at,std::vector<u8>(sizeof(T))};std::memcpy(s.bytes.data(),&value,sizeof(T));segments.push_back(s);}
void reset(){segments.clear();write_count=0;fail_read_address=0;}
}
Result pminfoInitialize(){return 0;}void pminfoExit(){}
Result smGetService(Service* s,const char*){s->active=true;return 0;}
bool serviceIsActive(Service* s){return s->active;}void serviceClose(Service* s){s->active=false;}
Result pmdmntGetApplicationProcessId(u64* p){*p=42;return 0;}
Result pminfoGetProgramId(u64* p,u64){*p=re4::profile::title_id;return 0;}
Result test_dispatch(Service*,unsigned command,const void* in,std::size_t,void*,std::size_t,TestDispatchParams p){
    if(command==65103){++write_count;return 9;}
    assert(command==65102);struct Range{u64 addr,size;};const auto r=*static_cast<const Range*>(in);
    if(r.addr==fail_read_address)return 9;
    for(const auto& s:segments)if(r.addr>=s.address&&r.addr-s.address<=s.bytes.size()&&r.size<=s.bytes.size()-(r.addr-s.address)){
        auto* out=const_cast<void*>(p.buffers[0].data);std::memcpy(out,s.bytes.data()+r.addr-s.address,r.size);
        if(r.addr==main_base+slot_offset){++slot_reads;if(slot_reads==fail_slot_read)return 9;if(slot_reads==2&&switch_slot){const u64 other=software+8;std::memcpy(out,&other,8);}}
        if(r.addr==software&&fail_final_id&&slot_reads>=2)return 9;
        if(r.addr==software&&switch_id&&slot_reads>=2){const u32 other=1;std::memcpy(out,&other,4);}
        return 0;
    }
    return 7;
}
namespace {
Metadata setup_renderer(){
    reset();main_base=0x7100000000;main_size=0x1200000;slot_reads=0;switch_slot=switch_id=fail_final_id=false;fail_slot_read=0;
    Metadata m{};m.title=re4::profile::title_id;m.main={main_base,main_size};std::memcpy(m.build_id,re4::profile::build_id,32);
    segment(main_base+slot_offset,software);segment(software,u32(0));
    std::array<float,80> matrices{};for(unsigned i=0;i<matrices.size();++i)matrices[i]=float(i);
    segment(software+0x4400C0,matrices);
    segment(software+0x440C90,std::array<float,6>{-1,0,512,448,0,-100});
    segment(software+0x440C70,std::array<float,6>{0,0,1280,720,0,1});return m;
}
void replace_segment(u64 at,const void* data,std::size_t size){for(auto& s:segments)if(s.address==at){assert(size==s.bytes.size());std::memcpy(s.bytes.data(),data,size);return;}assert(false);}
}
int main(){
    Service service{};re4::LiveHudRendererObservation o;auto m=setup_renderer();
    assert(read_live_hud_renderer(&service,m,o)&&o.context_valid&&o.context_stable&&!o.ordered&&o.context_index==0);
    assert(o.model.valid&&o.view.valid&&o.projection.valid&&o.correction.valid&&o.combined.valid);
    assert(o.model.values[0]==0&&o.view.values[0]==16&&o.projection.values[0]==32&&o.correction.values[0]==48&&o.combined.values[15]==79);
    assert(o.logical_viewport.valid&&o.adjusted_viewport.valid&&o.logical_viewport.values[0]==-1&&o.logical_viewport.values[5]==-100&&write_count==0);
    std::array<float,80> matrices{};matrices[17]=std::numeric_limits<float>::quiet_NaN();replace_segment(software+0x4400C0,matrices.data(),sizeof(matrices));
    assert((read_live_hud_renderer(&service,m,o)&&o.model.valid&&!o.view.valid&&o.projection.valid&&o.view.values==std::array<float,16>{}));
    matrices[17]=1e9f;replace_segment(software+0x4400C0,matrices.data(),sizeof(matrices));assert(read_live_hud_renderer(&service,m,o)&&o.view.valid);
    matrices[17]=1.01e9f;replace_segment(software+0x4400C0,matrices.data(),sizeof(matrices));assert(read_live_hud_renderer(&service,m,o)&&!o.view.valid);
    std::array<float,6> viewport{0,0,0,-8,0,-100};replace_segment(software+0x440C90,viewport.data(),sizeof(viewport));
    assert(read_live_hud_renderer(&service,m,o)&&o.logical_viewport.valid&&o.logical_viewport.values[2]==0&&o.logical_viewport.values[3]==-8);
    viewport[0]=65537;replace_segment(software+0x440C90,viewport.data(),sizeof(viewport));assert(read_live_hud_renderer(&service,m,o)&&!o.logical_viewport.valid&&o.adjusted_viewport.valid);
    m=setup_renderer();fail_read_address=software+0x440C90;assert(read_live_hud_renderer(&service,m,o)&&!o.logical_viewport.valid&&o.adjusted_viewport.valid&&o.model.valid);
    m=setup_renderer();fail_read_address=software+0x4400C0;assert(read_live_hud_renderer(&service,m,o)&&!o.model.valid&&!o.combined.valid&&o.logical_viewport.valid);
    for(u64 pointer:{u64(0),u64(0xFFF),software+1,std::numeric_limits<u64>::max()-7}){
        m=setup_renderer();replace_segment(main_base+slot_offset,&pointer,sizeof(pointer));assert(!read_live_hud_renderer(&service,m,o)&&!o.context_valid&&!o.model.valid);
    }
    m=setup_renderer();const u32 bad_id=1;replace_segment(software,&bad_id,sizeof(bad_id));assert(!read_live_hud_renderer(&service,m,o)&&!o.context_valid);
    m=setup_renderer();switch_slot=true;assert(!read_live_hud_renderer(&service,m,o)&&!o.context_valid&&!o.model.valid);
    m=setup_renderer();fail_slot_read=2;assert(!read_live_hud_renderer(&service,m,o)&&!o.context_valid&&!o.model.valid);
    m=setup_renderer();fail_final_id=true;assert(!read_live_hud_renderer(&service,m,o)&&!o.context_valid);
    m=setup_renderer();switch_id=true;assert(!read_live_hud_renderer(&service,m,o)&&!o.context_valid&&!o.logical_viewport.valid);
    m=setup_renderer();fail_read_address=software;assert(!read_live_hud_renderer(&service,m,o)&&!o.context_valid);
    m=setup_renderer();fail_read_address=main_base+slot_offset;assert(!read_live_hud_renderer(&service,m,o)&&!o.context_valid);
    m=setup_renderer();m.main.size=slot_offset+7;assert(!read_live_hud_renderer(&service,m,o)&&slot_reads==0);
    m=setup_renderer();m.main.base|=1;assert(!read_live_hud_renderer(&service,m,o)&&slot_reads==0);
    m=setup_renderer();m.main.size=std::numeric_limits<u64>::max();assert(!read_live_hud_renderer(&service,m,o)&&slot_reads==0);
    m=setup_renderer();m.build_id[0]^=1;assert(!read_live_hud_renderer(&service,m,o)&&slot_reads==0);
    m=setup_renderer();m.title^=1;assert(!read_live_hud_renderer(&service,m,o)&&slot_reads==0);
    m=setup_renderer();assert(!read_live_hud_renderer(nullptr,m,o)&&!o.context_valid&&slot_reads==0);
    assert(write_count==0);
}

