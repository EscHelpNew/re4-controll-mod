#pragma once
#include "gyro_test_background_image.hpp"

namespace re4::gyro_background {
static_assert(kWidth==1280 && kHeight==720);
static_assert(sizeof(pixels)/sizeof(pixels[0])==1280*720);
}
