#pragma once
#include <cstring>

namespace re4 {
// The pinned dispatcher's actual stack layout is FP[32] at +0x000, then
// GP[31] at +0x200. Its InlineFloatCtx C++ declaration describes the reverse
// ordering. Keep the dispatcher (and its pre-saved LR) intact; adapt the one
// register this project consumes using a byte copy, without aliasing the
// mismatched C++ type. tools/check_hook_abi.py verifies the compiled layout.
inline void scale_saved_s0(void* context, float factor) {
    float value;
    std::memcpy(&value, context, sizeof(value));
    value *= factor;
    std::memcpy(context, &value, sizeof(value));
}
}
