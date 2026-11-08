#pragma once
#include "re4/grenade_prediction.hpp"
#include "re4/reticle_draw.hpp"

namespace re4 {
// Four annuli, sixteen sectors: 64 native four-vertex strips, no texture or
// collision access. Each innermost strip has one intentional degenerate
// triangle and one center wedge; every other strip fills an annular quad.
inline bool draw_grenade_landing_spot(const ReticleDrawBackend& backend,const ViewMatrix3x4& view,
        const ViewMatrix4x4& projection,const GrenadeLanding& landing,float radius=200,float opacity=1){
    if(!backend.prepare_hud||!backend.blend||!backend.vertex_format||!backend.begin
        ||!backend.position||!backend.color||!backend.end||!grenade_world_point(landing.position)
        ||!std::isfinite(radius)||radius<=0||radius>2000
        ||!std::isfinite(opacity)||opacity<=0||opacity>1)return false;
    ViewVec3 normal;
    if(!grenade_unit_normal(landing.normal,normal)||normal.y<=.7f)return false;
    ViewVec3 checked;
    if(!view_projection_valid(projection)||!view_inverse_point(view,{},checked))return false;
    const double length=std::hypot(double(normal.x),double(normal.y));
    const ViewVec3 tangent{float(normal.y/length),float(-normal.x/length),0};
    const ViewVec3 bitangent{float(-normal.x*normal.z/length),float(-normal.y*normal.z/length),float(length)};
    const ViewVec3 center{landing.position.x+2*normal.x,landing.position.y+2*normal.y,landing.position.z+2*normal.z};
    ViewHudPoint points[5][17]{};
    for(unsigned ring=0;ring<=4;++ring){
        for(unsigned sector=0;sector<16;++sector){
            const double angle=double(sector)*6.2831853071795864769/16;
            const double along=radius*(double(ring)/4)*std::cos(angle);
            const double across=radius*(double(ring)/4)*std::sin(angle);
            ViewVec3 world;
            if(!view_store_point(center.x+along*tangent.x+across*bitangent.x,
                    center.y+along*tangent.y+across*bitangent.y,
                    center.z+along*tangent.z+across*bitangent.z,world)
                ||!grenade_world_point(world))return false;
            // Native27A140 stores m22=-near/(far-near),
            // m23=-near*far/(far-near), m32=-1: near maps to -w and
            // far to0. Preserve this native depth and camera-facing check
            // before flattening the projected geometry for the HUD.
            ViewVec3 camera;
            if(!view_transform_point(view,world,camera))return false;
            const double values[4]={camera.x,camera.y,camera.z,1};
            double clip[4]{};
            for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)clip[i]+=double(projection.m[i][j])*values[j];
            for(double value:clip)if(!std::isfinite(value))return false;
            if(camera.z>=0||clip[3]<=1e-6||clip[2] < -clip[3]||clip[2]>0)return false;
            const ViewHudPoint screen{float((clip[0]/clip[3]+1)*256),float((1-clip[1]/clip[3])*224)};
            if(!std::isfinite(screen.x)||!std::isfinite(screen.y))return false;
            // Retain true projected positions beyond x/y edges. The native HUD
            // orthographic projection clips these strips to the viewport. One
            // clipped outer vertex must not hide an otherwise visible disc.
            points[ring][sector]=screen;
        }
        points[ring][16]=points[ring][0];
    }
    float left=points[0][0].x,right=left,top=points[0][0].y,bottom=top;
    for(const auto& ring:points)for(const auto p:ring){
        left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);
    }
    if(right<=0||left>=512||bottom<=0||top>=448)return false;
    // Degenerate projected floors (for example a camera exactly on the plane)
    // are hidden. No state changes have been queued, even if a late point clips.
    for(unsigned sector=0;sector<16;++sector){
        const auto a=points[0][sector],b=points[4][sector],c=points[4][sector+1];
        const double area=(double(b.x)-a.x)*(double(c.y)-a.y)-(double(b.y)-a.y)*(double(c.x)-a.x);
        if(!std::isfinite(area)||std::fabs(area)<=1e-6)return false;
    }
    // Denser red center; retain the existing outer falloff and transparent edge.
    // Alpha interpolation within each strip joins rings continuously; edge=0.
    constexpr unsigned falloff[5]={92,59,27,7,0};
    // Linear RGB samples strengthen saturation toward the center.
    // Native per-vertex RGBA interpolation keeps the tint continuous too.
    constexpr unsigned tint[5][3]={{255,12,8},{249,21,16},{244,30,24},{238,39,32},{232,48,40}};
    unsigned alpha[5]{};
    for(unsigned ring=0;ring<=4;++ring)alpha[ring]=unsigned(std::lround(falloff[ring]*opacity));
    if(alpha[0]==0)return false;
    auto* const context=backend.context;
    backend.prepare_hud(context);
    backend.blend(context,1,4,5,0);
    backend.vertex_format(context,0,9,1,4,0);
    for(unsigned ring=0;ring<4;++ring){
        for(unsigned sector=0;sector<16;++sector){
            backend.begin(context,0x98,0,4);
            const ViewHudPoint vertices[]={points[ring][sector],points[ring+1][sector],
                points[ring][sector+1],points[ring+1][sector+1]};
            for(unsigned vertex=0;vertex<4;++vertex){
                backend.position(context,vertices[vertex].x,vertices[vertex].y,0);
                const unsigned color_ring=ring+vertex%2;
                backend.color(context,tint[color_ring][0],tint[color_ring][1],tint[color_ring][2],alpha[color_ring]);
            }
            backend.end(context,0x34);
        }
    }
    backend.blend(context,1,1,0,0);
    backend.vertex_format(context,0,9,1,3,0);
    return true;
}
}
