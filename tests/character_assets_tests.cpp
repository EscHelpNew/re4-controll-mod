#include "re4/character_assets.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace {
using namespace re4;
template<class T,std::size_t N>void put(std::array<std::uint8_t,N>& b,std::size_t p,T v){std::memcpy(b.data()+p,&v,sizeof(v));}
struct Fixture {
    alignas(32) std::array<std::uint8_t,0x400> bytes{};
    Fixture(){
        put<std::uint64_t>(bytes,0,0xA0);bytes[0x29]=26;
        put<std::uint64_t>(bytes,0x20,0x2A0);bytes[0x28]=2;
        put<std::uint16_t>(bytes,0x42,2);
        put<std::uint64_t>(bytes,0x48,0x240);put<std::uint64_t>(bytes,0x50,0x260);
        put<std::uint16_t>(bytes,0x58,2);put<std::uint16_t>(bytes,0x5A,2);
        put<std::uint64_t>(bytes,0x70,0x280);put<std::uint64_t>(bytes,0x78,0x288);
        for(unsigned i=0;i<26;++i){bytes[0xA0+i*16]=std::uint8_t(i);bytes[0xA1+i*16]=character_core_parents[i];put<float>(bytes,0xA4+i*16,float(i));}
        put<ViewVec3>(bytes,0x240,{1,2,3});put<ViewVec3>(bytes,0x24C,{4,5,6});
        put<ViewVec3>(bytes,0x260,{0,1,0});put<ViewVec3>(bytes,0x26C,{1,0,0});
        put<std::uint16_t>(bytes,0x280,0);put<std::uint16_t>(bytes,0x282,1);
        put<std::uint16_t>(bytes,0x288,1);put<std::uint16_t>(bytes,0x28A,0);
        put<CharacterWeight8>(bytes,0x2A0,{{4,0,0},1,{255,0,0},0});
        put<CharacterWeight8>(bytes,0x2A8,{{18,22,0},2,{30,255,0},0});
    }
    CharacterMutableBytes span(){return {bytes.data(),bytes.size()};}
};
}
int main(){
    using namespace re4;
    assert(!character_asset_recipe(CharacterAppearance::Leon));
    assert(!character_asset_recipe(static_cast<CharacterAppearance>(255)));
    const unsigned counts[]{7,6,6,6,6,2,4},bones[]{141,81,90,127,61,111,34};
    const unsigned right_bins[]{13,13,6,14,14,0,444},left_bins[]{16,14,7,16,15,0,449};
    const unsigned hand_tpls[]{1,12,1,13,2,0,441};
    static_assert(character_asset_piece_limit==7);
    for(unsigned i=1;i<=7;++i){
        const auto* r=character_asset_recipe(static_cast<CharacterAppearance>(i));
        assert(r&&unsigned(r->appearance)==i&&r->piece_count==counts[i-1]&&r->body_bone_count==bones[i-1]);
        assert(!std::strncmp(r->path,"bio4/em/",8)&&r->size>0&&r->size<=character_archive_limit);
        unsigned hash=0;for(auto b:r->sha256)hash|=b;assert(hash);
        unsigned right=0,left=0,knife=0;
        for(unsigned j=0;j<r->piece_count;++j){
            const auto& p=r->pieces[j];
            if(p.role==CharacterAssetRole::RightHand){++right;assert(p.bin==right_bins[i-1]&&p.tpl==hand_tpls[i-1]);}
            if(p.role==CharacterAssetRole::LeftHand){++left;assert(p.bin==left_bins[i-1]&&p.tpl==hand_tpls[i-1]);}
            if(p.role==CharacterAssetRole::Knife){++knife;assert(i==4&&p.bin==9&&p.tpl==5&&!(p.flags_clear&8));}
        }
        assert(right==(i==6?0u:1u)&&left==right&&knife==(i==4?1u:0u));
        CharacterAssetReady ready;ready.piece_count=77;
        assert(!character_asset_ready(r->appearance,nullptr,r->size,ready)&&ready.piece_count==77);
        alignas(32)std::array<std::uint8_t,32> bad{};
        assert(!character_asset_ready(r->appearance,bad.data(),bad.size(),ready)&&ready.piece_count==77);
    }
    CharacterAssetReady ready;ready.piece_count=77;
    assert(!character_asset_ready(CharacterAppearance::Leon,nullptr,0,ready)&&ready.piece_count==77);
    // These are invented format records, not game assets. Exercise the bounded
    // parser independently; only character_asset_ready authenticates readiness.
    Fixture f;CharacterAssetPiece piece;
    assert(character_asset_detail::bin(f.span(),26,piece));
    assert(piece.weight_count==2&&piece.vertex_count==2&&piece.normal_count==2);
    assert(piece.weights.data==f.bytes.data()+0x2A0&&piece.weights.size==16);
    assert(piece.role==CharacterAssetRole::Body);
    assert(!character_asset_detail::apply_role(piece,CharacterAssetRole::RightHand));
    assert(piece.role==CharacterAssetRole::Body);
    auto hand=f;
    put<CharacterWeight8>(hand.bytes,0x2A0,{{10,0,0},1,{100,0,0},0});
    put<CharacterWeight8>(hand.bytes,0x2A8,{{9,10,0},2,{30,255,0},0});
    CharacterAssetPiece hand_piece;
    assert(character_asset_detail::bin(hand.span(),26,hand_piece));
    assert(character_asset_detail::apply_role(hand_piece,CharacterAssetRole::RightHand));
    assert(hand_piece.role==CharacterAssetRole::RightHand);
    assert(!character_asset_detail::apply_role(hand_piece,CharacterAssetRole::LeftHand));
    assert(hand_piece.role==CharacterAssetRole::RightHand);
    assert(!character_asset_detail::apply_role(hand_piece,static_cast<CharacterAssetRole>(255)));
    assert(!character_asset_detail::apply_role(hand_piece,CharacterAssetRole::Knife));
    hand_piece.weight_count=1;hand_piece.weights.size=8;
    assert(character_asset_detail::apply_role(hand_piece,CharacterAssetRole::Knife));
    assert(hand_piece.role==CharacterAssetRole::Knife);
    // Identical admission bound for gameplay and inventory deformation slots.
    CharacterAssetReady capacity;
    CharacterAssetPiece large;large.vertex_count=large.normal_count=29120;
    assert(character_asset_detail::add_deformed(capacity,large));
    assert(capacity.deformed_slot_bytes==character_asset_deformed_slot_limit);
    assert(2u*3u*capacity.deformed_slot_bytes<=2u*2u*1024u*1024u);
    const auto full=capacity.deformed_slot_bytes;large.vertex_count=1;large.normal_count=0;
    assert(!character_asset_detail::add_deformed(capacity,large)&&capacity.deformed_slot_bytes==full);
    std::array<CharacterBindBone,character_bone_limit> bind{};
    assert(character_asset_detail::bind({f.bytes.data(),f.bytes.size()},26,bind.data()));
    assert(bind[25].id==25&&bind[25].parent==24&&bind[25].position.x==25);
    auto reject=[&](const Fixture& bad){
        auto copy=bad;CharacterAssetPiece output;output.weight_count=99;
        assert(!character_asset_detail::bin(copy.span(),26,output)&&output.weight_count==99);
    };
    auto bad=f;put<std::uint64_t>(bad.bytes,0x20,std::numeric_limits<std::uint64_t>::max());reject(bad);
    bad=f;put<std::uint64_t>(bad.bytes,0x48,0x3FC);reject(bad);
    bad=f;put<std::uint64_t>(bad.bytes,0x48,0x241);reject(bad);
    bad=f;put<float>(bad.bytes,0x260,std::numeric_limits<float>::infinity());reject(bad);
    bad=f;put<std::uint16_t>(bad.bytes,0x282,2);reject(bad);
    bad=f;put<std::uint16_t>(bad.bytes,0x288,2);reject(bad);
    bad=f;bad.bytes[0x2A3]=0;reject(bad);
    bad=f;bad.bytes[0x2A0]=26;reject(bad);
    bad=f;bad.bytes[0x2AB]=3;bad.bytes[0x2AC]=80;bad.bytes[0x2AD]=30;reject(bad);
    bad=f;put<std::uint16_t>(bad.bytes,0x42,256);reject(bad);
    bad=f;bad.bytes[0xA0+25*16]=26;reject(bad);
    bad=f;put<float>(bad.bytes,0xA4,std::numeric_limits<float>::quiet_NaN());reject(bad);
    assert(!character_asset_detail::bin({f.bytes.data()+1,f.bytes.size()-1},26,piece));
    for(std::size_t size=0;size<0x98;++size)assert(!character_asset_detail::bin({f.bytes.data(),size},26,piece));
    // Sparse piece skeletons have global IDs, and can start at a nonzero root.
    bad=f;bad.bytes[0x29]=1;bad.bytes[0xA0]=14;bad.bytes[0xA1]=255;
    assert(character_asset_detail::bin(bad.span(),26,piece));
    assert(!character_asset_detail::bind({bad.bytes.data(),bad.bytes.size()},26,bind.data()));
    // Sparse face pieces can repeat a global bone as a new local root. These
    // records are never used to allocate the canonical retarget skeleton.
    bad=f;bad.bytes[0xA0+2*16]=1;bad.bytes[0xA1+2*16]=255;
    assert(character_asset_detail::bin(bad.span(),26,piece));
    assert(!character_asset_detail::bind({bad.bytes.data(),bad.bytes.size()},26,bind.data()));
    alignas(32)std::array<std::uint8_t,0x100> tpl{};
    put<std::uint32_t>(tpl,0,0x12345678);put<std::uint32_t>(tpl,4,1);
    put<std::uint64_t>(tpl,8,0x10);put<std::uint64_t>(tpl,0x10,0x20);
    put<std::uint64_t>(tpl,0x28,0x60);put<std::uint32_t>(tpl,0x60,0x01000001);
    put<std::uint32_t>(tpl,0x64,123);
    assert(character_asset_detail::tpl({tpl.data(),tpl.size()},0x01000001));
    CharacterAssetReady textures;
    assert(character_asset_detail::tpl({tpl.data(),tpl.size()},0x01000001,&textures));
    assert(textures.texture_count==1&&textures.texture_ids[0]==123);
    assert(character_asset_detail::tpl({tpl.data(),tpl.size()},0x01000001,&textures));
    assert(textures.texture_count==1); // Shared TPLs do not consume another slot.
    alignas(32)auto second=tpl;put<std::uint32_t>(second,0x64,456);
    assert(character_asset_detail::tpl({second.data(),second.size()},0x01000001,&textures));
    assert(textures.texture_count==2&&textures.texture_ids[1]==456);
    const auto before_ids=textures.texture_ids;
    assert(!character_asset_detail::tpl({tpl.data(),tpl.size()},0x01000002,&textures));
    assert(textures.texture_count==2&&textures.texture_ids==before_ids);
    textures.texture_count=256;for(unsigned i=0;i<256;++i)textures.texture_ids[i]=i;
    assert(!character_asset_detail::tpl({second.data(),second.size()},0x01000001,&textures));
    assert(textures.texture_count==256&&textures.texture_ids[255]==255);
    assert(character_asset_detail::tpl({tpl.data(),tpl.size()},0x01000001,&textures)); // Existing ID at capacity.
    textures.texture_count=257;
    assert(!character_asset_detail::tpl({tpl.data(),tpl.size()},0x01000001,&textures));
    alignas(32)std::array<std::uint8_t,0x100> two_tpl{};
    put<std::uint32_t>(two_tpl,0,0x12345678);put<std::uint32_t>(two_tpl,4,2);
    put<std::uint64_t>(two_tpl,8,0x10);
    put<std::uint64_t>(two_tpl,0x10,0x40);put<std::uint64_t>(two_tpl,0x20,0x70);
    put<std::uint64_t>(two_tpl,0x48,0xA0);put<std::uint64_t>(two_tpl,0x78,0xA8);
    put<std::uint32_t>(two_tpl,0xA0,0x01000001);put<std::uint32_t>(two_tpl,0xA8,0x01000002);
    put<std::uint32_t>(two_tpl,0xA4,255);put<std::uint32_t>(two_tpl,0xAC,256);
    textures.texture_count=255;for(unsigned i=0;i<256;++i)textures.texture_ids[i]=i;
    const auto initial_ids=textures.texture_ids;
    assert(!character_asset_detail::tpl({two_tpl.data(),two_tpl.size()},0x01000001,&textures));
    assert(textures.texture_count==255&&textures.texture_ids==initial_ids); // Late wrong pack is atomic.
    put<std::uint32_t>(two_tpl,0xA8,0x01000001);
    assert(!character_asset_detail::tpl({two_tpl.data(),two_tpl.size()},0x01000001,&textures));
    assert(textures.texture_count==255&&textures.texture_ids==initial_ids); // Late capacity failure is atomic.
    assert(!character_asset_detail::tpl({tpl.data(),tpl.size()},0x01000002));
    alignas(32)auto bad_tpl=tpl;put<std::uint64_t>(bad_tpl,0x28,0xFD);
    assert(!character_asset_detail::tpl({bad_tpl.data(),bad_tpl.size()},0x01000001));
    bad_tpl=tpl;put<std::uint64_t>(bad_tpl,0x10,0xF8);
    assert(!character_asset_detail::tpl({bad_tpl.data(),bad_tpl.size()},0x01000001));
    bad_tpl=tpl;put<std::uint32_t>(bad_tpl,0,0);
    assert(!character_asset_detail::tpl({bad_tpl.data(),bad_tpl.size()},0x01000001));
    assert(!character_asset_detail::tpl({tpl.data()+1,tpl.size()-1},0x01000001));
    // Full-size but wrong bytes must fail the fixed installed digest gate.
    const auto* ashley=character_asset_recipe(CharacterAppearance::Ashley);
    std::vector<std::uint8_t> storage(ashley->size+31);
    auto address=(reinterpret_cast<std::uintptr_t>(storage.data())+31)&~std::uintptr_t(31);
    assert(!character_asset_ready(CharacterAppearance::Ashley,reinterpret_cast<std::uint8_t*>(address),ashley->size,ready)&&ready.piece_count==77);
    std::puts("character assets: fixed recipes, authentication gate, bounded palettes/geometry and sparse binds PASS");
}
