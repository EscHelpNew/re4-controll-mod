#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace re4 {
constexpr std::size_t kButtonCount = 20;
constexpr std::uint8_t kUnboundButton = 255;
using ButtonMap = std::array<std::uint8_t, kButtonCount>;

constexpr ButtonMap identity_button_map() {
    ButtonMap map{};
    for (unsigned i = 0; i < kButtonCount; ++i) map[i] = std::uint8_t(i);
    return map;
}
constexpr bool valid_button_map(const ButtonMap& map) {
    for (auto target : map)
        if (target >= kButtonCount && target != kUnboundButton) return false;
    return true;
}
constexpr std::uint64_t button_mask(unsigned index) {
    return index < 16 ? std::uint64_t{1} << index
         : index < kButtonCount ? std::uint64_t{1} << (index + 8) : 0;
}
inline const char* button_name(unsigned index) {
    constexpr const char* names[] = {"A", "B", "X", "Y", "L3", "R3", "L", "R",
        "ZL", "ZR", "PLUS", "MINUS", "D-left", "D-up", "D-right", "D-down",
        "SL(L)", "SR(L)", "SL(R)", "SR(R)"};
    return index < kButtonCount ? names[index] : "Unbound";
}
constexpr std::uint64_t remap_buttons(std::uint64_t buttons, const ButtonMap& map, bool enabled = true) {
    if (!enabled) return buttons;
    std::uint64_t result = buttons & ~std::uint64_t{0x0f00ffff};
    for (unsigned i = 0; i < kButtonCount; ++i)
        if (buttons & button_mask(i)) result |= button_mask(map[i]);
    return result;
}
}
