#include "re5/camera_probe.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
static unsigned allocations=0;
void* operator new(std::size_t n){++allocations;if(auto* p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"camera probe line%d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
int main(){
 re5::CameraProbe p;std::atomic_flag lock=ATOMIC_FLAG_INIT;re5::initialize_camera_probe(p);re5::CameraProbeRecorder recorder{p,lock};
 CHECK(!std::memcmp(p.marker,re5::kCameraProbeMarker,16)&&p.abi==1&&p.size==5808&&p.sequence==0&&p.calls==0);
 const auto allocations_before=allocations;const auto initial=p;re5::CameraProbeInput input{};input.raw={1,2,3,32767};input.pitch=.23f;input.frame=9;
 CHECK(!recorder.input(false,1,0,input)&&!std::memcmp(&p,&initial,sizeof(p)));
 re5::CameraProbeDecision d{};d.stage=std::uint32_t(re5::CameraProbeStage::Component);d.reason=std::uint32_t(re5::CameraProbeReason::Applied);d.input_frame=9;
 CHECK(!recorder.decision(false,1,0,d)&&!std::memcmp(&p,&initial,sizeof(p)));
 CHECK(recorder.input(true,1,0,input)&&p.sequence==2&&p.calls==1&&p.request_sequence==1);
 CHECK(p.players[0].last_input.raw[3]==32767&&p.players[0].last_nonzero_ry.frame==9&&input.call==1&&input.request_sequence==1);
 CHECK(recorder.decision(true,1,0,d)&&p.sequence==4&&p.players[0].last_success.call==2&&p.players[0].reasons[d.reason]==1);
 input.raw[3]=0;input.frame=10;CHECK(recorder.input(true,1,0,input)&&p.players[0].last_input.frame==10&&p.players[0].last_nonzero_ry.frame==9);
 d.reason=std::uint32_t(re5::CameraProbeReason::WorldNotCommitted);d.expected_world[12]=5;d.world[12]=6;d.world_again[12]=6;d.available=re5::CameraHasExpectedWorld|re5::CameraHasWorld|re5::CameraHasWorldAgain;
 CHECK(recorder.decision(true,1,0,d)&&p.players[0].last_failure.world[12]==6&&p.players[0].last_success.reason==std::uint32_t(re5::CameraProbeReason::Applied));
 const auto retained=p;CHECK(!recorder.decision(false,1,0,d)&&!std::memcmp(&retained,&p,sizeof(p)));
 CHECK(!lock.test_and_set());CHECK(!recorder.decision(true,1,0,d)&&!std::memcmp(&retained,&p,sizeof(p)));CHECK(lock.test_and_set());lock.clear();
 // A new request resets only its histogram; useful records keep old tags.
 d.reason=std::uint32_t(re5::CameraProbeReason::SamePose);CHECK(recorder.decision(true,2,1,d));
 CHECK(p.request_sequence==2&&p.players[0].reasons[std::uint32_t(re5::CameraProbeReason::Applied)]==0&&p.players[0].last_success.request_sequence==1);
 CHECK(p.players[0].last_nonzero_ry.raw[3]==32767&&p.players[1].last_component.reason==std::uint32_t(re5::CameraProbeReason::SamePose));
 d.stage=std::uint32_t(re5::CameraProbeStage::PostDesired);d.reason=std::uint32_t(re5::CameraProbeReason::PostDesired);d.native.eye={1,2,3};
 CHECK(recorder.decision(true,2,1,d)&&p.players[1].last_post_desired.native.eye[2]==3&&p.players[1].last_component.reason==std::uint32_t(re5::CameraProbeReason::SamePose));
 d.available=0;d.reason=std::uint32_t(re5::CameraProbeReason::NoFreshFrame);CHECK(recorder.decision(true,2,2,d)&&p.unrouted_calls==1&&p.last_unrouted.reason==d.reason);
 CHECK(p.players[0].reasons[d.reason]==0&&p.players[1].reasons[d.reason]==0&&!(p.sequence&1));
 // Native lease authorization is rechecked after lock acquisition. An old
 // lease may not mutate sequence, retained payload or histogram tags.
 struct Lease{bool active;unsigned request,calls;};Lease lease{true,3,0};
 auto valid=[](void* context,std::uint32_t request){auto& value=*static_cast<Lease*>(context);++value.calls;return value.active&&request==value.request;};
 re5::CameraProbeRecorder gated{p,lock,&lease,valid};const auto before_gate=p;
 CHECK(!gated.decision(true,2,0,d)&&lease.calls==1&&!std::memcmp(&p,&before_gate,sizeof(p)));
 lease.active=false;CHECK(!gated.input(true,3,0,input)&&lease.calls==2&&!std::memcmp(&p,&before_gate,sizeof(p)));
 CHECK(!gated.input(false,3,0,input)&&lease.calls==2);lease.active=true;CHECK(gated.input(true,3,0,input)&&p.request_sequence==3);
 CHECK(allocations==allocations_before);
 std::puts("RE5 camera probe: fixed POD, default Off, one-try lock, retained samples, request histograms and both-player separation PASS");
}
