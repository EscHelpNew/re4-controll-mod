#include "re5/reticle.hpp"
#include <cassert>
#include <cstdio>
int main(){
 re5::Matrix view,projection;for(unsigned i=0;i<4;++i)view.m[i][i]=projection.m[i][i]=1;
 float point[3]={0,0,.5f};re5::ScreenPoint out;re5::Viewport full{0,0,1280,720},second{0,360,1280,720};
 assert(re5::project(view,projection,point,full,out)&&out.x==640&&out.y==360);
 assert(re5::project(view,projection,point,second,out)&&out.x==640&&out.y==540);
 point[0]=.5f;view.m[3][0]=-.5f;assert(re5::project(view,projection,point,second,out)&&out.x==640); // Row translation.
 projection.m[3][3]=-1;assert(!re5::project(view,projection,point,full,out));projection.m[3][3]=1;
 point[2]=-1;assert(!re5::project(view,projection,point,full,out));point[2]=.5f;point[0]=2;assert(!re5::project(view,projection,point,full,out));
 std::array<re5::MarkerRect,4> rectangles;
 assert(re5::marker_rectangles({640,540},second,2,1,rectangles)==4&&rectangles[0].x==629&&rectangles[2].height==8);
 assert(re5::marker_rectangles({640,360},second,2,1,rectangles)==0);assert(re5::marker_rectangles({640,540},second,1,2,rectangles)==1&&rectangles[0].width==4);
 std::puts("RE5 row projection, split-screen viewport and bounded marker geometry PASS");
}
