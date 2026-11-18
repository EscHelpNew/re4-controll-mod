#pragma once
#include <cstdint>
namespace re4 {
enum class GameTarget { RE4=0, RE6=1, RE5=2 };
inline constexpr std::uint64_t game_title_id(GameTarget game){
 return game==GameTarget::RE4?0x010099A00BC1E000ULL:game==GameTarget::RE5?0x010018100CD46000ULL:game==GameTarget::RE6?0x01002A000CD48000ULL:0;
}
inline constexpr const char* game_name(GameTarget game){return game==GameTarget::RE4?"RE4":game==GameTarget::RE5?"RE5":game==GameTarget::RE6?"RE6":"Unknown";}
}
