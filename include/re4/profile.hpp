#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace re4::profile {
constexpr std::uint64_t title_id = 0x010099A00BC1E000;
constexpr std::size_t build_id_offset = 0xAE1BBC;
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
