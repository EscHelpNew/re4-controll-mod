#pragma once
#include <cstdint>
namespace re4 {
enum class CharacterAppearance : std::uint8_t {
    Leon, Ashley, Ada, LuisSera, Krauser, Merchant, RamonSalazar, VillageGanado
};
constexpr bool valid_character_appearance(CharacterAppearance value){return unsigned(value)<8;}
enum class CharacterAppearanceStatus : std::uint32_t { Native, Pending, Active, Failed };
enum class CharacterAppearanceError : std::uint32_t {
    None, MountUnavailable, FileOpen, FileSize, FileRead, AssetMismatch,
    UnsupportedRig, NativeInitialization, RendererUnavailable, SkinningFailed,
    TextureUnavailable, RendererChanged,
    // Pending diagnostics share the existing snapshot field; they are never
    // persistent failures and do not change the shared-memory layout.
    WaitingRenderer=100, WaitingPlayer, WaitingScene, WaitingState,
    WaitingModel, WaitingBones, WaitingChain, WaitingAlternate,
    WaitingMaterial, WaitingEffect, WaitingCharacter, WaitingMount,
    WaitingRetirement, ReadingAsset, WaitingPackCommand, WaitingPackBusy,
    WaitingPackIO, WaitingProbeFence, WaitingUploadFence,
    WaitingReservations, WaitingSkin, WaitingDraw, WaitingHands,
    WaitingFrameSlot, WaitingRenderLock, WaitingNativeSkin, WaitingSlotConsumer,
    WaitingBinBones
};
// Carry one bounded observed byte without changing the shared snapshot layout.
// Zero upper bits mean no observation; +1 also represents an observed zero.
constexpr CharacterAppearanceError character_appearance_diagnostic(CharacterAppearanceError reason,std::uint8_t observed){
    return static_cast<CharacterAppearanceError>((std::uint32_t(reason)&0xffffu)|((std::uint32_t(observed)+1)<<16));
}
constexpr CharacterAppearanceError character_appearance_reason(std::uint32_t diagnostic){
    return static_cast<CharacterAppearanceError>(diagnostic&0xffffu);
}
constexpr int character_appearance_observed(std::uint32_t diagnostic){
    const auto encoded=diagnostic>>16;
    return encoded&&encoded<=256?int(encoded)-1:-1;
}
struct CharacterAppearanceSnapshot {
    CharacterAppearance applied=CharacterAppearance::Leon;
    CharacterAppearanceStatus status=CharacterAppearanceStatus::Native;
    std::uint32_t error=0,generation=0;
    bool valid=false;
};
}
