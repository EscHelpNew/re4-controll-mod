#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace re5 {
struct CameraPose {std::array<float,3> eye{},up{},target{};};
namespace camera_detail {
using Vec=std::array<float,3>;
inline Vec add(Vec a,Vec b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
inline Vec subtract(Vec a,Vec b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
inline Vec scale(Vec v,float factor){for(float& x:v)x*=factor;return v;}
inline float dot(Vec a,Vec b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec cross(Vec a,Vec b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline bool finite(Vec v){for(float x:v)if(!std::isfinite(x)||std::fabs(x)>1000000.f)return false;return true;}
inline Vec yaw(Vec v,float angle){const float c=std::cos(angle),s=std::sin(angle);return {c*v[0]+s*v[2],v[1],-s*v[0]+c*v[2]};}
inline Vec rotate(Vec v,Vec axis,float angle){const float c=std::cos(angle),s=std::sin(angle);return add(add(scale(v,c),scale(cross(axis,v),s)),scale(axis,dot(axis,v)*(1-c)));}
}
// Transform the complete native arm around a root-relative upper-body anchor.
// This retains its screen composition, native arm dimensions and roll, while
// body heading and overview pitch own the view before native wall collision.
inline bool anchored_camera_pose(const CameraPose& native,std::array<float,3> anchor,float forward_x,float forward_z,
 float pitch,float distance,CameraPose& out){
 using namespace camera_detail;
 if(!finite(native.eye)||!finite(native.target)||!finite(native.up)||!finite(anchor)||!std::isfinite(pitch)||std::fabs(pitch)>.610866f
  ||!std::isfinite(distance)||distance<1||distance>2||!std::isfinite(forward_x)||!std::isfinite(forward_z))return false;
 const auto direction=subtract(native.target,native.eye);const float length=std::sqrt(dot(direction,direction)),horizontal=std::hypot(direction[0],direction[2]);
 const float forward_length=std::hypot(forward_x,forward_z),up_cross=std::sqrt(dot(cross(direction,native.up),cross(direction,native.up)));
 if(!std::isfinite(length)||length<.001f||horizontal<.001f||forward_length<.001f||!std::isfinite(up_cross)||up_cross<=length*.000001f)return false;
 forward_x/=forward_length;forward_z/=forward_length;
 const float turn=std::atan2(forward_x,forward_z)-std::atan2(direction[0],direction[2]);
 const float elevation=std::atan2(direction[1],horizontal),tilt=pitch-elevation;
 const Vec right{-forward_z,0,forward_x};
 auto rotate_rig=[&](Vec v){return rotate(yaw(v,turn),right,tilt);};
 const auto arm=rotate_rig(subtract(native.eye,anchor)),look=rotate_rig(direction),up=rotate_rig(native.up);
 CameraPose result;result.eye=add(anchor,scale(arm,distance));result.target=add(result.eye,look);result.up=up;
 if(!finite(result.eye)||!finite(result.target)||!finite(result.up))return false;
 out=result;return true;
}

// Dilate the native eye about the anatomical anchor, then translate its
// target by the same displacement to retain native direction, length and roll.
inline bool dilated_camera_pose(const CameraPose& native,std::array<float,3> anchor,float distance,CameraPose& out){
 using namespace camera_detail;
 if(!finite(native.eye)||!finite(native.target)||!finite(native.up)||!finite(anchor)
  ||!std::isfinite(distance)||distance<1||distance>2)return false;
 const auto look=subtract(native.target,native.eye);const float length=std::sqrt(dot(look,look));
 if(!std::isfinite(length)||length<.001f||std::sqrt(dot(cross(look,native.up),cross(look,native.up)))<=length*.000001f)return false;
 CameraPose result=native;
 for(unsigned i=0;i<3;++i){result.eye[i]=float(double(anchor[i])+(double(native.eye[i])-anchor[i])*distance);
  result.target[i]=float(double(result.eye[i])+(double(native.target[i])-native.eye[i]));}
 if(!finite(result.eye)||!finite(result.target))return false;
 out=result;return true;
}
namespace camera_detail {
struct OpticalBasis {std::array<double,3> right{},up{},forward{},relative{};double depth=0,tangent=0;};
inline bool optical_basis(const CameraPose& pose,Vec anchor,float fov,float aspect,OpticalBasis& out){
 if(!finite(pose.eye)||!finite(pose.target)||!finite(pose.up)||!finite(anchor)
  ||!std::isfinite(fov)||fov<=1||fov>=179||!std::isfinite(aspect)||aspect<=.01f||aspect>10)return false;
 auto dot3=[](const auto& a,const auto& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
 for(unsigned i=0;i<3;++i){out.forward[i]=double(pose.target[i])-pose.eye[i];out.relative[i]=double(anchor[i])-pose.eye[i];}
 const double length=std::sqrt(dot3(out.forward,out.forward));if(!std::isfinite(length)||length<.001)return false;
 for(double& x:out.forward)x/=length;
 const auto& f=out.forward;const auto& u=pose.up;
 out.right={f[1]*u[2]-f[2]*u[1],f[2]*u[0]-f[0]*u[2],f[0]*u[1]-f[1]*u[0]};
 const double width=std::sqrt(dot3(out.right,out.right));if(!std::isfinite(width)||width<.000001)return false;
 for(double& x:out.right)x/=width;
 const auto& r=out.right;out.up={r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
 out.depth=dot3(out.relative,out.forward);out.tangent=std::tan(double(fov)*3.14159265358979323846/360);
 return std::isfinite(out.depth)&&out.depth>.001&&std::isfinite(out.tangent)&&out.tangent>0;
}
}
inline bool camera_anchor_projection(const CameraPose& pose,std::array<float,3> anchor,float fov,float aspect,std::array<double,2>& out){
 camera_detail::OpticalBasis basis;if(!camera_detail::optical_basis(pose,anchor,fov,aspect,basis))return false;
 double x=0,y=0;for(unsigned i=0;i<3;++i){x+=basis.relative[i]*basis.right[i];y+=basis.relative[i]*basis.up[i];}
 out={x/(basis.depth*basis.tangent*aspect),y/(basis.depth*basis.tangent)};
 return std::isfinite(out[0])&&std::isfinite(out[1])&&std::fabs(out[0])<=16&&std::fabs(out[1])<=16;
}
inline bool composed_camera_pose(const CameraPose& native,std::array<float,3> anchor,float fov,float aspect,
 const std::array<double,2>& wanted,CameraPose& out){
 camera_detail::OpticalBasis basis;if(!camera_detail::optical_basis(native,anchor,fov,aspect,basis)
  ||!std::isfinite(wanted[0])||!std::isfinite(wanted[1])||std::fabs(wanted[0])>16||std::fabs(wanted[1])>16)return false;
 double x=0,y=0;for(unsigned i=0;i<3;++i){x+=basis.relative[i]*basis.right[i];y+=basis.relative[i]*basis.up[i];}
 x-=wanted[0]*basis.depth*basis.tangent*aspect;y-=wanted[1]*basis.depth*basis.tangent;
 CameraPose result=native;
 for(unsigned i=0;i<3;++i){const double delta=basis.right[i]*x+basis.up[i]*y;
  result.eye[i]=float(double(native.eye[i])+delta);
  // Use the same effective translation after rounding the eye. This retains
  // the native look vector at game-world float precision.
  result.target[i]=float(double(result.eye[i])+(double(native.target[i])-native.eye[i]));}
 if(!camera_detail::finite(result.eye)||!camera_detail::finite(result.target))return false;
 out=result;return true;
}


struct CameraPoseIdentity {std::uintptr_t camera=0,actor=0,command=0;unsigned local_player=0;};
// A one-use native desired-pose baseline. The caller restores before native
// desired/history evaluation, independently of current overlay/input gates.
class CameraPoseRestore {
 bool live_=false;CameraPoseIdentity owner_{};CameraPose native_{};std::uint64_t tick_=0;
 static bool valid(CameraPoseIdentity owner,const CameraPose& pose){
  if(!owner.camera||!owner.actor||!owner.command||owner.local_player>=2)return false;
  for(const auto* values:{&pose.eye,&pose.up,&pose.target})for(float x:*values)if(!std::isfinite(x)||std::fabs(x)>1000000.f)return false;
  return true;
 }
public:
 bool pending(std::uintptr_t camera)const{return live_&&owner_.camera==camera;}
 bool remember(CameraPoseIdentity owner,const CameraPose& pose,std::uint64_t tick){
  // A missed try-lock restoration leaves the only genuine native baseline
  // pending. Skip another overlay instead of capturing its customized output.
  if(live_&&owner.camera==owner_.camera)return false;
  clear();if(!valid(owner,pose)||!tick)return false;
  owner_=owner;native_=pose;tick_=tick;live_=true;return true;
 }
 bool take(CameraPoseIdentity owner,std::uint64_t tick,CameraPose& out){
  const bool usable=live_&&owner.camera==owner_.camera&&owner.actor==owner_.actor&&owner.command==owner_.command
   &&owner.local_player==owner_.local_player&&tick>=tick_&&tick-tick_<=1920000;
  clear();if(!usable)return false;
  out=native_;return true;
 }
 void clear(){live_=false;}
};
}
