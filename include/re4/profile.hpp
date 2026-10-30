#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace re4::profile {
constexpr std::uint64_t title_id = 0x010099A00BC1E000;
constexpr std::size_t build_id_offset = 0xAE1BBC;
constexpr std::size_t player_slot = 0x113B398;
constexpr std::size_t input_flags = 0x124EC50;
constexpr std::size_t movement_dispatch = 0x30D430;
constexpr std::uint8_t build_id[32] = {
    0x82,0xC2,0xA0,0x4A,0x21,0xD3,0xE0,0xB8,0x50,0xF2,0x73,0xD4,0x14,0x9C,0x90,0x03
};
struct Site { std::size_t offset; std::uint32_t instruction; };
constexpr Site sites[] = {
    {0x00220BCC, 0x9410CE61}, {0x00220C10, 0x9410CE54},
    {0x00220C54, 0x9410CE47}, {0x0025F680, 0xD10143FF},
    {0x0030D84C, 0x36100188}, {0x0030D870, 0xBD40B661},
    {0x0030D8F0, 0xBD40B661}, {0x0030D9DC, 0x361000E8},
    {0x0030D9EC, 0xBD40B661}, {0x0030DA10, 0xBD40B661},
    {0x0030DD8C, 0x371000A8}, {0x0030DDCC, 0x900040C8}
    , {movement_dispatch, 0x395A3008}, {0x2B8F10, 0xB940D409},
    {0x19B20, 0xD10703FF}, {0x33DF80, 0xFC1C0FEA}, {0x317630, 0x39445408},
    {0x33EC50, 0xFC1C0FE8}, {0x18520,0xD10543FF}, {0x19CCC,0x97FFFA15},
    {0x187D0,0xBD427288}, {0x18920,0xBD427680}, {0x19CB0,0x1E210800},
    {0x19CBC,0x1E210803}, {0x6E8900,0x3F666666},
    {0x64C3A0,0xD10283FF}, {0x64C558,0xD63F0100},
    {0x24FAE0,0x7114001F}, {0x24FAE8,0x710B403F},
    {0x4F5C90,0xF81D0FF5}, {0x4F5C94,0xA9014FF4},
    {0x4EB700,0xF81C0FF7}, {0x4ECFB0,0xD104C3FF},
    {0x6F6330,1920}, {0x6F6334,1080}, {0x6F6338,1600}, {0x6F633C,900},
    {0x6F6340,1280}, {0x6F6344,720}, {0x6F6348,1066}, {0x6F634C,600},
    {0x23EFC0,0x32000BE5}, {0x647454,0x120002A7}
};
inline bool matches(const std::uint8_t* memory, std::size_t size) {
    if (!memory || size < build_id_offset + sizeof(build_id)
        || std::memcmp(memory + build_id_offset, build_id, sizeof(build_id))) return false;
    for (const auto site : sites) {
        std::uint32_t value;
        if (site.offset + sizeof(value) > size) return false;
        std::memcpy(&value, memory + site.offset, sizeof(value));
        if (value != site.instruction) return false;
    }
    return true;
}
}
