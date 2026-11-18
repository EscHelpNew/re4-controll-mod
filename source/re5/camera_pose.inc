// Restore the last owned native desired pose before native transition/history
// evaluation. This cleanup intentionally does not depend on fresh input or
// the current camera settings/component; those gates govern only new writes.
struct CameraPoseSlot {std::atomic_flag lock=ATOMIC_FLAG_INIT;re5::CameraPoseRestore state;};
CameraPoseSlot camera_poses[2];
HOOK_DEFINE_INLINE(Re5CameraPoseRestore){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto camera=re4::saved_x(ctx,19);invalidate_camera_follow(camera);
 for(unsigned i=0;i<2;++i){auto& slot=camera_poses[i];
  if(slot.lock.test_and_set(std::memory_order_acquire))continue;
  struct Unlock{std::atomic_flag& flag;~Unlock(){flag.clear(std::memory_order_release);}} unlock{slot.lock};
  if(!slot.state.pending(camera))continue;
  std::uintptr_t actor=0,table=0;re5::ResolvedPlayer player;
  if(!memory.get(camera,0,table)||table!=main_base+re5::profile::offset<0x1AD2510>()
   ||!memory.get(camera,0x130,actor)||!actor_player(actor,player)||player.local_player!=i){slot.state.clear();continue;}
  re5::CameraPose original;
  if(!slot.state.take({camera,actor,player.command,i},svcGetSystemTick(),original))continue;
  if(!writable(nullptr,camera+0x60,12)||!writable(nullptr,camera+0x70,12)||!writable(nullptr,camera+0x80,12))continue;
  std::memcpy(reinterpret_cast<void*>(camera+0x60),original.eye.data(),12);
  std::memcpy(reinterpret_cast<void*>(camera+0x70),original.up.data(),12);
  std::memcpy(reinterpret_cast<void*>(camera+0x80),original.target.data(),12);
 }
}};
