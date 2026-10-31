#pragma once
#include "re4/graphics_preview.hpp"
#include "graphics_preview_images.hpp"
namespace graphics_preview {
static_assert(kImageWidth==320&&kImageHeight==180);
static_assert(sizeof(re4_pixels)/sizeof(re4_pixels[0])==320*180);
static_assert(sizeof(re6_pixels)/sizeof(re6_pixels[0])==320*180);
inline Image image(re4::GameTarget game){
 if(game==re4::GameTarget::RE4)return {re4_pixels,320*180,320,180};
 if(game==re4::GameTarget::RE6)return {re6_pixels,320*180,320,180};
 return {};
}
}
