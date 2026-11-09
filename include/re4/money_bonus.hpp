#pragma once
#include <cstdint>
#include "re4/settings.hpp"

namespace re4 {
inline constexpr std::int32_t kMoneyBonus=200000,kMoneyMaximum=99999999;
enum class MoneyResult : std::uint32_t {None,Granted,AlreadyClaimed,Unavailable,StorageError,Capacity};
enum class MoneyAvailability : std::uint32_t {Unavailable,Ready,Claimed,StorageError,Capacity};
struct MoneySnapshot {
    std::uint32_t request=0,ack=0;
    MoneyResult result=MoneyResult::None;
    MoneyAvailability availability=MoneyAvailability::Unavailable;
    bool pending()const{return request!=ack;}
};
inline const char* money_action_label(Language language){return language==Language::Russian?"Получить 200 000 песет":"Receive 200 000 pesetas";}
inline const char* money_status_text(const MoneySnapshot& value,Language language){
    const bool ru=language==Language::Russian;
    if(value.pending())return ru?"Начисление…":"Pending…";
    switch(value.availability){
        case MoneyAvailability::Ready:return ru?"Доступно":"Available";
        case MoneyAvailability::Claimed:return ru?"Уже получено":"Already received";
        case MoneyAvailability::StorageError:return ru?"Ошибка SD-карты":"SD card error";
        case MoneyAvailability::Capacity:return ru?"Недостаточно места в кошельке":"Wallet limit";
        default:return ru?"Недоступно":"Unavailable";
    }
}
}
