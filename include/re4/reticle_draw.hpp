#pragma once
#include "re4/view.hpp"
#include <cmath>

namespace re4 {
// Native GX wrappers enqueue their arguments into the engine command stream.
// prepare_hud must establish the late 29D140 HUD state (orthographic 512x448,
// identity model matrix, direct xyz/RGBA8 vertices, no texture, native depth/cull/
// channel states). The selected no-texture shader passes alpha unchanged.
// It restores the two states it changes: opaque blend and signed-short xyz.
// It does not query the asynchronously executed renderer or touch aim/camera.
struct ReticleDrawBackend {
    void* context=nullptr;
    void (*prepare_hud)(void*)=nullptr;
    void (*blend)(void*,unsigned,unsigned,unsigned,unsigned)=nullptr;
    void (*vertex_format)(void*,unsigned,unsigned,unsigned,unsigned,unsigned)=nullptr;
    void (*begin)(void*,unsigned,unsigned,unsigned)=nullptr;
    void (*position)(void*,float,float,float)=nullptr;
    void (*color)(void*,unsigned,unsigned,unsigned,unsigned)=nullptr;
    void (*end)(void*,unsigned)=nullptr;
};

// style: 1 dot, 2 crosshair; size: 0 small, 1 medium, 2 large. Coordinates
// are supplied by the caller in native late-HUD 512x448 logical pixels.
inline bool draw_reticle(const ReticleDrawBackend& backend,ViewHudPoint center,
        unsigned style,unsigned size){
    if((style!=1&&style!=2)||size>2||!std::isfinite(center.x)||!std::isfinite(center.y)
        ||!backend.prepare_hud||!backend.blend||!backend.vertex_format||!backend.begin
        ||!backend.position||!backend.color||!backend.end)return false;
    struct Rect {float left,top,right,bottom;};
    Rect rectangles[4]{};
    unsigned count=1;
    if(style==1){
        const float half=float(2+size)*0.5f;
        rectangles[0]={center.x-half,center.y-half,center.x+half,center.y+half};
    }else{
        const float length=float(6+size*2),half=size?1.0f:0.5f,gap=float(2+size);
        count=4;
        rectangles[0]={center.x-gap-length,center.y-half,center.x-gap,center.y+half};
        rectangles[1]={center.x+gap,center.y-half,center.x+gap+length,center.y+half};
        rectangles[2]={center.x-half,center.y-gap-length,center.x+half,center.y-gap};
        rectangles[3]={center.x-half,center.y+gap,center.x+half,center.y+gap+length};
    }
    // Validate the entire marker before enqueuing any state or geometry.
    for(unsigned i=0;i<count;++i){
        const auto& r=rectangles[i];
        if(!(r.left>=0&&r.top>=0&&r.right<=512&&r.bottom<=448
            &&r.left<r.right&&r.top<r.bottom))return false;
    }
    auto* const context=backend.context;
    backend.prepare_hud(context);
    // Same alpha blend as native 4BABE0 selector1: source alpha/inverse alpha.
    backend.blend(context,1,4,5,0);
    backend.vertex_format(context,0,9,1,4,0); // xyz float, no fractional bits.
    for(unsigned i=0;i<count;++i){
        const auto& r=rectangles[i];
        backend.begin(context,0x98,0,4); // Native four-vertex triangle strip.
        const ViewHudPoint vertices[]={{r.left,r.top},{r.right,r.top},
            {r.left,r.bottom},{r.right,r.bottom}};
        for(const auto& vertex:vertices){
            backend.position(context,vertex.x,vertex.y,0);
            backend.color(context,255,255,255,128);
        }
        // Native float quad completion token, as emitted at4BADF8.
        backend.end(context,0x34);
    }
    backend.blend(context,1,1,0,0);
    backend.vertex_format(context,0,9,1,3,0);
    return true;
}
}
