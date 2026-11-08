#pragma once
#include "re4/live_renderer.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
namespace {
// Authenticated 24FB20 resolves context index0 through main+1184150. These are
// software caches read outside its command stream; no atomic draw-state claim.
bool read_live_hud_renderer(Service* service,const Metadata& metadata,re4::LiveHudRendererObservation& out){
    out={};
    constexpr u64 context_slot=0x1184150,matrix_offset=0x4400C0;
    constexpr u64 logical_offset=0x440C90,adjusted_offset=0x440C70;
    constexpr u64 last_byte=logical_offset+6*sizeof(float);
    constexpr auto maximum=std::numeric_limits<u64>::max();
    u64 context=0;
    if(!service||!supported(metadata)||!read_main_pointer(service,metadata,context_slot,context)
        ||context>maximum-last_byte)return false;
    u32 index=0;
    if(R_FAILED(read_memory(service,context,&index,sizeof(index)))||index!=0)return false;
    re4::LiveHudRendererObservation next{};
    std::array<float,80> matrices{};
    if(R_SUCCEEDED(read_memory(service,context+matrix_offset,matrices.data(),sizeof(matrices)))){
        re4::LiveHudMatrix* destinations[]={&next.model,&next.view,&next.projection,&next.correction,&next.combined};
        for(unsigned i=0;i<5;++i){
            bool valid=true;
            for(unsigned j=0;j<16;++j)if(!std::isfinite(matrices[i*16+j])||std::fabs(matrices[i*16+j])>1e9f){valid=false;break;}
            if(valid){destinations[i]->valid=true;std::memcpy(destinations[i]->values.data(),matrices.data()+i*16,16*sizeof(float));}
        }
    }
    const auto viewport=[&](u64 offset,re4::LiveHudViewport& destination){
        std::array<float,6> values{};
        if(R_FAILED(read_memory(service,context+offset,values.data(),sizeof(values))))return;
        for(unsigned i=0;i<6;++i)if(!std::isfinite(values[i])||std::fabs(values[i])>(i<4?65536.f:1e9f))return;
        destination.valid=true;destination.values=values;
    };
    viewport(logical_offset,next.logical_viewport);viewport(adjusted_offset,next.adjusted_viewport);
    // Detect context-slot replacement and changed software identity. Fields may
    // still race ordinary writes in-place; they remain explicitly unordered.
    u64 after=0;u32 after_index=0;
    if(!read_main_pointer(service,metadata,context_slot,after)||after!=context
        ||R_FAILED(read_memory(service,context,&after_index,sizeof(after_index)))||after_index!=index)return false;
    next.context_valid=true;next.context_stable=true;next.context_index=index;
    out=next;return true;
}
}
