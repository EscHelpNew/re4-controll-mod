#pragma once
#include "re4/character_appearance.hpp"
#include <cstddef>
#include <cstdint>
namespace re4 {
enum class CharacterLoadingStage : std::uint8_t {
    None, Context, Mapping, RegistryCapacity, ResidentEntry, IoState,
    Thread, Prefix, CompressedVariant, FileOpen, FileSize, Allocation,
    Heap, Headroom, Revalidation, Handshake, NativeIo, Blob, Metadata,
    DescriptorIndex, DescriptorOwner, DescriptorCapacity, Creator, UniqueCount
};
// Existing failure10 occupies low16. Stage and a bounded observation occupy
// separate upper bytes. Observation means MiB only for stageHeadroom.
constexpr std::uint32_t character_loading_diagnostic(CharacterLoadingStage stage,unsigned observed=0){
    return std::uint32_t(CharacterAppearanceError::TextureUnavailable)
        |(std::uint32_t(stage)<<16)|((observed>255?255u:observed)<<24);
}
constexpr CharacterLoadingStage character_loading_stage(std::uint32_t diagnostic){
    return character_appearance_reason(diagnostic)==CharacterAppearanceError::TextureUnavailable
        ?static_cast<CharacterLoadingStage>((diagnostic>>16)&255):CharacterLoadingStage::None;
}
constexpr unsigned character_loading_observed(std::uint32_t diagnostic){return diagnostic>>24;}
constexpr unsigned character_loading_mebibytes(std::size_t bytes){
    const auto amount=bytes/(1024*1024);return amount>255?255u:unsigned(amount);
}
constexpr std::uint8_t character_loading_percent(std::size_t completed,std::size_t total){
    // Model archives are capped at9MiB. Division first also avoids overflow if
    // this helper is passed an arbitrary host-size input by a snapshot reader.
    if(!total)return 0;
    if(completed>=total)return 100;
    const auto unit=total/100,remainder=total%100;
    unsigned percent=0;
    while(percent<99){
        const auto next=percent+1;
        const auto threshold=unit*next+(remainder*next+99)/100;
        if(completed<threshold)break;
        percent=next;
    }
    return std::uint8_t(percent);
}
}
