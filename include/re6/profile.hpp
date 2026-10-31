#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "re6/journal.hpp"

// RE6 Switch update v131072. All addresses are image-relative, verified from
// hash-checked local NSOs; extracted images are never part of the product.
namespace re6::profile {
inline constexpr std::uint64_t title_id=0x01002A000CD48000ULL;
inline constexpr std::size_t main_size=0x328F000,sdk_size=0xC77000;
inline constexpr std::size_t build_id_offset=0x25A3054,sdk_build_id_offset=0xB52AD8;
inline constexpr BuildId build_id={0x06,0x1D,0x2A,0x06,0xCD,0x70,0x07,0x85,0xE9,0x87,0xC4,0x2C,0xE2,0xD7,0x22,0x31};
inline constexpr BuildId sdk_build_id={0xB2,0x87,0x2A,0x7B,0xCC,0x58,0x99,0x69,0xFB,0x7A,0xFB,0x8E,0x37,0x24,0x14,0xE4,0xFA,0xC5,0xA7,0x50};
inline constexpr std::size_t camera_update=0x16038E0,camera_singleton=0x2A5F018,player_singleton=0x28EAC10,view_override_singleton=0x2A60810;
inline constexpr std::size_t sdk_full=0x23CC80,sdk_handheld=0x23CD50,sdk_dual=0x23D400;
inline constexpr std::uint64_t input_max_age=kTickHz/10; // 100 ms, reported poll timestamp remains exact.
struct Site {std::size_t offset;std::uint32_t instruction;};
inline constexpr Site main_sites[]={
 {0x16038E0,0xD10303FF},{0x16038E4,0xFD0023EA},{0x16038E8,0x6D0523E9},
 {0x16038EC,0xA9066FFC},{0x16038F0,0xA90767FA},{0x16038F4,0xA9085FF8},
 // Native manager matrix writes and viewport-to-actor/camera selection.
 {0x16039CC,0xA94E2768},{0x1603AA0,0xB900E368},{0x1603B34,0xB9012368},
 {0x332C7C,0xB948C909},{0x332C80,0xB90D7E69},{0x332D44,0xF9401908},{0x332D4C,0xF95D1518}
 ,{0x1673C20,0xD10383FF},{0x1673C24,0x6D062BEB},{0x1673C28,0x6D0723E9},
 {0x1673C2C,0xA9086FFC},{0x1673C30,0xA90967FA},{0x1673C34,0xA90A5FF8},
 {0x1673CA4,0xB9400288},{0x1673CF4,0x29406D5A},{0x1673CF8,0x5316650A},
 {0x1673D00,0x7100453F},{0x1673D08,0x7100213F},{0x1673D10,0x53115508},{0x1673DDC,0x340000FC},
 {0x1234840,0xD10443FF},{0x1234844,0x6D092BEB},{0x1234848,0x6D0A23E9},
 {0x123484C,0xA90B6FFC},{0x1234850,0xA90C67FA},{0x1234854,0xA90D5FF8},
 {0x12348BC,0x2A0203F5},{0x12348C0,0xAA0103F4},
 {0x1236C34,0xB945E688},{0x1236C38,0xB9001FE8},{0x1236C3C,0x51000508},{0x1236C40,0x2A0803F7},
 {0x1236C44,0x71000D1F},{0x1236C48,0x540001C8},{0x1237368,0xB9401FE8},
 {0x1237370,0x71000D1F},{0x1237374,0x54000488},{0x12376E0,0x90008108},
 {0x12376E4,0x9128E108},{0x12376EC,0xB8775908},{0x1237760,0x94108814},{0x1237D84,0xD65F03C0}
};
inline constexpr Site sdk_sites[]={
 {sdk_full,0xD10083FF},{sdk_full+4,0xA9017BFD},{sdk_full+8,0x910043FD},
 {sdk_full+12,0xAA0103E8},{sdk_full+16,0xAA0003E9},{sdk_full+20,0xD10013A0},
 {sdk_handheld,0xD10083FF},{sdk_handheld+4,0xA9017BFD},{sdk_handheld+8,0x910043FD},
 {sdk_handheld+12,0xAA0103E8},{sdk_handheld+16,0xAA0003E9},{sdk_handheld+20,0xD10013A0},
 {sdk_dual,0xD10083FF},{sdk_dual+4,0xA9017BFD},{sdk_dual+8,0x910043FD},
 {sdk_dual+12,0xAA0103E8},{sdk_dual+16,0xAA0003E9},{sdk_dual+20,0xD10013A0}
};
template<std::size_t N> inline bool check(const std::uint8_t* memory,std::size_t size,std::size_t minimum,
 std::size_t offset,const BuildId& expected,const Site (&sites)[N]){
 if(!memory||size<minimum||offset>size||size-offset<expected.size()||std::memcmp(memory+offset,expected.data(),expected.size()))return false;
 for(const auto site:sites){std::uint32_t value=0;if(site.offset>size||size-site.offset<4)return false;std::memcpy(&value,memory+site.offset,4);if(value!=site.instruction)return false;}
 return true;
}
inline bool matches(const std::uint8_t* memory,std::size_t size){return check(memory,size,main_size,build_id_offset,build_id,main_sites);}
inline bool sdk_matches(const std::uint8_t* memory,std::size_t size){return check(memory,size,sdk_size,sdk_build_id_offset,sdk_build_id,sdk_sites);}
}
