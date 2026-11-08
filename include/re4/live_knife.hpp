#pragma once
#include <array>
#include <cstdint>

namespace re4 {
struct LiveKnifePart {
 bool present=false,valid=false,visible=false,next_present=false;
 bool bin_readable=false,tpl_readable=false,weapon_bin=false,weapon_tpl=false;
 // Fields come from the copied part; matrix_finite includes its runtime1e9 bound.
 bool fields_readable=false,vtable_in_main=false,matrix_finite=false;
 std::uint32_t vtable_offset=0,bin_flags=0;
 std::uint16_t animation=0,bin_vertices=0,bin_normals=0;
 std::uint8_t bin_header_3b=0;
 std::uint32_t flags=0;
 std::uint8_t alpha=0,bin_bones=0;
 std::int32_t bin_entry=-1,tpl_entry=-1;
};
// Samples cover only the first float3, never the whole native arrays. stable
// rechecks context identity/current slot; it is not an ordered/atomic snapshot.
struct LiveKnifeBladeBuffers {
 bool slot_readable=false,slot_valid=false,stable=false;
 std::uint32_t slot=0;
 bool positions_present=false,positions_readable=false,positions_finite=false;
 bool normals_present=false,normals_readable=false,normals_finite=false;
};
struct LiveKnifeBody {
 bool valid=false,stable=false,face_present=false;
 bool right_hand_present=false,left_hand_present=false;
 bool aliases_right_hand=false,aliases_left_hand=false;
 bool chain_valid=false,chain_cycle=false,chain_truncated=false,face58_member=false;
 bool directory_valid=false,directory_complete=false;
 std::uint32_t chain_count=0;
 std::int32_t face58_index=-1;
 LiveKnifePart face58{};
 // Copied metadata matches the ordinary blade shape; this does not prove
 // complete runtime admission, whole-array validity or ordered skinning.
 bool source_shape_matches=false;
 LiveKnifeBladeBuffers buffers{};
};
struct LiveKnifeHeld {
 bool present=false,valid=false,stable=false,aliases_other=false,vtable_in_main=false;
 bool native_knife_class=false,parent_is_player=false,actor_visible=false,control_hidden=false;
 bool root_present=false,root_valid=false,root_parent_is_grip=false,root_matrix_valid=false;
 bool bin_bones_match=false;
 std::uint32_t vtable_offset=0,flags=0;
 std::uint8_t type=0,bones=0,control=0;
 std::array<float,12> root_matrix{};
 LiveKnifePart part{};
};
// Fixed copied observations only. Native pointers remain local to the reader.
// A relative vtable offset is meaningful only when vtable_in_main is true.
// valid and stable are independent; every subsection has separate validity.
struct LiveKnifeObservation {
 bool valid=false,stable=false,player_valid=false,player_stable=false,vtable_in_main=false;
 bool state_valid=false,asset_matches_state70=false,grip_valid=false,budget_exhausted=false;
 std::uint32_t player_vtable_offset=0,player_flags=0,read_calls=0,read_bytes=0;
 std::uint8_t player_type=0,player_bones=0,native_character=0,weapon_id=0,weapon_sub=0;
 std::array<std::uint8_t,4> routine{};
 LiveKnifeBody body{};
 std::array<LiveKnifeHeld,2> held{};
};
}
