#pragma once
#include "re4/character_appearance.hpp"
#include "re4/character_archive.hpp"
#include "re4/character_digest.hpp"
#include "re4/character_retarget.hpp"
#include <array>
#include <algorithm>

namespace re4 {
constexpr unsigned character_asset_piece_limit=7;
constexpr std::size_t character_asset_deformed_slot_limit=0xAAA00;
enum class CharacterAssetRole : std::uint8_t {Body,RightHand,LeftHand,Knife};
struct CharacterMutableBytes {std::uint8_t* data=nullptr;std::size_t size=0;};
struct CharacterAssetPieceRecipe {
    unsigned bin=0,tpl=0;
    std::uint32_t flags_or=0,flags_clear=0;
    CharacterAssetRole role=CharacterAssetRole::Body;
};
struct CharacterAssetRecipe {
    CharacterAppearance appearance=CharacterAppearance::Leon;
    const char* path=nullptr;
    unsigned file_index=0;
    std::uint32_t texture_pack=0;
    std::size_t size=0;
    std::array<std::uint8_t,32> sha256{};
    unsigned body_bone_count=0,piece_count=0;
    std::array<CharacterAssetPieceRecipe,character_asset_piece_limit> pieces{};
};
struct CharacterAssetPiece {
    CharacterMutableBytes bin{},tpl{},weights{},position_weights{},normal_weights{};
    unsigned weight_count=0,vertex_count=0,normal_count=0,bone_record_count=0;
    std::uint32_t flags_or=0,flags_clear=0;
    CharacterAssetRole role=CharacterAssetRole::Body;
};
struct CharacterAssetReady {
    CharacterAppearance appearance=CharacterAppearance::Leon;
    CharacterMutableBytes raw{},directory{};
    unsigned body_bone_count=0,piece_count=0;
    unsigned texture_count=0,total_vertex_count=0,total_normal_count=0;
    std::size_t deformed_slot_bytes=0;
    std::array<std::uint32_t,256> texture_ids{};
    std::array<CharacterBindBone,character_bone_limit> body_bind{};
    std::array<CharacterAssetPiece,character_asset_piece_limit> pieces{};
};

namespace character_asset_detail {
constexpr unsigned nibble(char x){return x>='a'?unsigned(x-'a'+10):unsigned(x-'0');}
constexpr std::array<std::uint8_t,32> hash(const char (&hex)[65]){
    std::array<std::uint8_t,32> value{};
    for(unsigned i=0;i<32;++i)value[i]=std::uint8_t(nibble(hex[i*2])*16+nibble(hex[i*2+1]));
    return value;
}
inline bool aligned(const void* p,unsigned alignment){return p&&(reinterpret_cast<std::uintptr_t>(p)&(alignment-1))==0;}
inline CharacterBytes immutable(CharacterMutableBytes bytes){return {bytes.data,bytes.size};}
inline CharacterMutableBytes mutable_span(CharacterBytes bytes){return {const_cast<std::uint8_t*>(bytes.data),bytes.size};}
inline bool array(CharacterBytes bytes,std::size_t field,std::uint64_t count,std::uint64_t stride,
                  unsigned alignment,CharacterBytes& out){
    std::uint64_t offset=0;
    if(!character_read(bytes,field,offset)||offset<0x98||offset%alignment
        ||count>bytes.size/stride||!character_subspan(bytes,offset,count*stride,out))return false;
    return aligned(out.data,alignment);
}
inline bool bone(CharacterBytes bytes,unsigned i,CharacterBindBone& out){
    CharacterBytes row;
    if(!character_subspan(bytes,std::uint64_t(i)*16,16,row))return false;
    CharacterBindBone value;value.id=row.data[0];value.parent=row.data[1];
    if(!character_read(row,4,value.position)||!view_finite(value.position))return false;
    out=value;return true;
}
// Sparse mesh bind records use global IDs. Only the main body's table is dense.
inline bool bind(CharacterBytes bytes,unsigned expected,CharacterBindBone* output){
    std::uint8_t count=0;CharacterBytes table;
    if(!output||expected<26||expected>character_bone_limit||!character_read(bytes,0x29,count)
        ||count!=expected||!array(bytes,0,count,16,4,table))return false;
    std::array<CharacterBindBone,character_bone_limit> value{};
    for(unsigned i=0;i<count;++i)if(!bone(table,i,value[i]))return false;
    ViewVec3 positions[character_bone_limit];
    if(!character_bind_positions(value.data(),count,positions))return false;
    std::copy_n(value.data(),count,output);return true;
}
inline bool bin(CharacterMutableBytes input,unsigned body_count,CharacterAssetPiece& out){
    const auto bytes=immutable(input);
    if(!aligned(input.data,32)||bytes.size<0x98||body_count<26||body_count>character_bone_limit)return false;
    std::uint8_t records=0,short_count=0;std::uint16_t extended_count=0,nv=0,nn=0;
    std::uint32_t flags=0;
    if(!character_read(bytes,0x29,records)||!records||!character_read(bytes,0x28,short_count)
        ||!character_read(bytes,0x42,extended_count)||!character_read(bytes,0x58,nv)
        ||!character_read(bytes,0x5A,nn)||!nv||!nn||!character_read(bytes,0x38,flags)
        ||(flags&0x100000))return false;
    const unsigned count=std::max<unsigned>(short_count,extended_count);
    if(!count||count>255)return false;
    CharacterBytes table,weights,positions,normals,wi,ni;
    if(!array(bytes,0,records,16,4,table)||!array(bytes,0x20,count,8,4,weights)
        ||!array(bytes,0x48,nv,12,4,positions)||!array(bytes,0x50,nn,12,4,normals)
        ||!array(bytes,0x70,nv,2,2,wi)||!array(bytes,0x78,nn,2,2,ni))return false;
    for(unsigned i=0;i<records;++i){
        CharacterBindBone b;
        // Face-piece records may repeat global IDs as separate local roots.
        // Only the dense canonical body table defines the retarget skeleton.
        if(!bone(table,i,b)||b.id>=body_count
            ||(b.parent!=255&&b.parent>=b.id))return false;
    }
    for(unsigned i=0;i<count;++i){
        CharacterWeight8 w;if(!character_read(weights,i*8,w)||w.links<1||w.links>3)return false;
        unsigned used=0;
        for(unsigned j=0;j<w.links;++j){
            if(w.bone[j]>=body_count)return false;
            if(j+1<w.links){if(w.percent[j]>100-used)return false;used+=w.percent[j];}
        }
    }
    for(unsigned array_id=0;array_id<2;++array_id){
        const auto geometry=array_id?normals:positions,indices=array_id?ni:wi;
        const unsigned length=array_id?nn:nv;
        for(unsigned i=0;i<length;++i){
            ViewVec3 p;std::uint16_t index=0;
            if(!character_read(geometry,i*12,p)||!view_finite(p)
                ||!character_read(indices,i*2,index)||index>=count)return false;
        }
    }
    CharacterAssetPiece value;value.bin=input;value.weights=mutable_span(weights);
    value.position_weights=mutable_span(wi);value.normal_weights=mutable_span(ni);
    value.weight_count=count;value.vertex_count=nv;value.normal_count=nn;value.bone_record_count=records;
    out=value;return true;
}
// Role admission is performed only after the bounded BIN validator. Static
// donor hands follow the unchanged native wrist/grip; they never use Leon's
// glove geometry or unrelated donor secondary IDs.
inline bool apply_role(CharacterAssetPiece& piece,CharacterAssetRole role){
    if(role!=CharacterAssetRole::Body&&role!=CharacterAssetRole::RightHand
        &&role!=CharacterAssetRole::LeftHand&&role!=CharacterAssetRole::Knife)return false;
    if(role!=CharacterAssetRole::Body){
        if(!piece.weights.data||!piece.weight_count||piece.weight_count>255
            ||piece.weights.size<std::size_t(piece.weight_count)*sizeof(CharacterWeight8))return false;
        if(role==CharacterAssetRole::Knife&&piece.weight_count!=1)return false;
        for(unsigned i=0;i<piece.weight_count;++i){
            CharacterWeight8 weight;std::memcpy(&weight,piece.weights.data+i*sizeof(weight),sizeof(weight));
            if(weight.links<1||weight.links>3)return false;
            for(unsigned j=0;j<weight.links;++j){
                const unsigned bone=weight.bone[j];
                if(role==CharacterAssetRole::RightHand&&(bone!=9&&bone!=10))return false;
                if(role==CharacterAssetRole::LeftHand&&(bone!=15&&bone!=16))return false;
                if(role==CharacterAssetRole::Knife&&(bone!=10||weight.links!=1))return false;
            }
        }
    }
    piece.role=role;return true;
}
inline bool add_deformed(CharacterAssetReady& ready,const CharacterAssetPiece& piece){
    const auto aligned=[](unsigned count){return (std::uint64_t(count)*12+31)&~std::uint64_t(31);};
    const std::uint64_t bytes=aligned(piece.vertex_count)+aligned(piece.normal_count);
    if(ready.deformed_slot_bytes>character_asset_deformed_slot_limit
        ||bytes>character_asset_deformed_slot_limit-ready.deformed_slot_bytes)return false;
    ready.deformed_slot_bytes+=std::size_t(bytes);return true;
}
inline bool tpl(CharacterBytes bytes,std::uint32_t expected_pack,CharacterAssetReady* output=nullptr){
    if(!aligned(bytes.data,32)||bytes.size<16)return false;
    std::array<std::uint32_t,256> ids{};unsigned unique=0;
    if(output){
        if(output->texture_count>ids.size())return false;
        ids=output->texture_ids;unique=output->texture_count;
    }
    std::uint32_t magic=0,count=0;std::uint64_t offset=0;
    if(!character_read(bytes,0,magic)||magic!=0x12345678||!character_read(bytes,4,count)
        ||!count||count>256||!character_read(bytes,8,offset)||offset<16||offset%8)return false;
    CharacterBytes table;
    if(!character_subspan(bytes,offset,std::uint64_t(count)*16,table))return false;
    for(unsigned i=0;i<count;++i){
        std::uint64_t image=0,reference=0;std::uint32_t pack=0,id=0;CharacterBytes header,ref;
        if(!character_read(table,i*16,image)||image%8||!character_subspan(bytes,image,0x28,header)
            ||!character_read(header,8,reference)||reference%4
            ||!character_subspan(bytes,reference,8,ref)||!character_read(ref,0,pack)||!character_read(ref,4,id)
            ||pack!=expected_pack)return false;
        if(output){
            unsigned j=0;while(j<unique&&ids[j]!=id)++j;
            if(j==unique){if(unique==ids.size())return false;ids[unique++]=id;}
        }
    }
    if(output){output->texture_ids=ids;output->texture_count=unique;}
    return true;
}
}

// Installed Switch resources for the audited game build. These fingerprints
// describe complete raw UDAS bytes, before any native BIN/TPL relocation.
inline constexpr std::array<CharacterAssetRecipe,7> character_asset_recipes{{
    {CharacterAppearance::Ashley,"bio4/em/pl01.udas",0x22,0x01000001,1312768,
        character_asset_detail::hash("421ed30b405a7b68ff4ce1e88fb49c92ce35555da0454bd3a1f5c51986d7b09b"),141,7,
        {{{0,1,0,0},{3,5,0,0},{2,7,0,0},{4,1,0x40,0},{6,1,0,0},
          {13,1,0,0,CharacterAssetRole::RightHand},{16,1,0,0,CharacterAssetRole::LeftHand}}}},
    {CharacterAppearance::Ada,"bio4/em/pl0b.udas",0xA8,0x0100000B,2008000,
        character_asset_detail::hash("cd300fbc3a0f4e260c973da4f1a05e102fb4619cb0d3575f752a3d5c890a6e63"),81,6,
        {{{0,1,0,0},{2,3,0,0},{4,3,0,0},{5,6,0x40,0},
          {13,12,0,0,CharacterAssetRole::RightHand},{14,12,0,0,CharacterAssetRole::LeftHand}}}},
    {CharacterAppearance::LuisSera,"bio4/em/pl14.udas",0x49,0x01000014,903168,
        character_asset_detail::hash("4dc81eab4be5591781216c943fcfeea36aafa49e3acae95492b98d7b3a980000"),90,6,
        {{{0,1,0,0},{2,3,0,0},{4,3,0,0},{5,1,0,0},
          {6,1,0,0,CharacterAssetRole::RightHand},{7,1,0,0,CharacterAssetRole::LeftHand}}}},
    {CharacterAppearance::Krauser,"bio4/em/pl0a.udas",0xC5,0x0100000A,1678848,
        character_asset_detail::hash("56e7f6cdc509d592557103fbc5e8c6dfddb50ebc4703d5049e97cd360e141bc1"),127,6,
        {{{0,1,0,0},{2,3,0,0},{4,5,0x20,0},{9,5,0,0,CharacterAssetRole::Knife},
          {14,13,0,0,CharacterAssetRole::RightHand},{16,13,0,0,CharacterAssetRole::LeftHand}}}},
    {CharacterAppearance::Merchant,"bio4/em/em18.udas",0x34,0x00000018,489728,
        character_asset_detail::hash("150749c1bf29c016bcf7d29cf82d1228d1b86c41af8d638eb9fe25b8816e47c4"),61,6,
        {{{1,2,0,0},{10,11,0x20,0},{3,4,0,0},{8,11,0x20,0},
          {14,2,0,0,CharacterAssetRole::RightHand},{15,2,0,0,CharacterAssetRole::LeftHand}}}},
    {CharacterAppearance::RamonSalazar,"bio4/em/em34.udas",0x64,0x00000034,1788672,
        character_asset_detail::hash("20e7b10c01fd3ea37796e25fbd56bb3c1680463269d5888ce4e48fbd139b30ae"),111,2,
        {{{5,7,0,0},{6,7,0,0}}}},
    {CharacterAppearance::VillageGanado,"bio4/em/em10.udas",0x18,0x00000010,8903328,
        character_asset_detail::hash("816abf8173b2af325a052695f665f16bff6dd95e0a8c909c33d2e1d4f1680152"),34,4,
        {{{440,441,0,0},{442,441,8,0},
          {444,441,0,0,CharacterAssetRole::RightHand},{449,441,0,0,CharacterAssetRole::LeftHand}}}}
}};
constexpr const CharacterAssetRecipe* character_asset_recipe(CharacterAppearance appearance){
    const unsigned index=unsigned(appearance);
    return index>=1&&index<=character_asset_recipes.size()?&character_asset_recipes[index-1]:nullptr;
}

// No allocations and no native calls. Caller exclusively owns the aligned raw
// bank and must keep it immutable during authentication/validation. On success,
// keep the WHOLE bank alive while native pieces, palettes or TPLs refer to it.
// Returned pointers remain valid after in-place relocation; this validator must
// never run again on relocated bytes. Leon uses native resources and has no bank.
inline bool character_asset_ready(CharacterAppearance appearance,std::uint8_t* raw,
                                 std::size_t size,CharacterAssetReady& out){
    const auto* recipe=character_asset_recipe(appearance);
    if(!recipe||size!=recipe->size||!character_asset_detail::aligned(raw,32))return false;
    const auto begin=reinterpret_cast<std::uintptr_t>(raw),destination=reinterpret_cast<std::uintptr_t>(&out);
    if(begin<=destination?destination-begin<size:begin-destination<sizeof(out))return false;
    CharacterDigest digest;std::array<std::uint8_t,32> actual{};
    if(!digest.update(raw,size)||!digest.finish(actual.data())||actual!=recipe->sha256)return false;
    // Exact fixed byte identity gates all remaining native format internals.
    CharacterArchive archive;
    if(!character_archive({raw,size},archive))return false;
    CharacterAssetReady value;value.appearance=appearance;value.raw={raw,size};
    value.directory=character_asset_detail::mutable_span(archive.dat);
    value.body_bone_count=recipe->body_bone_count;value.piece_count=recipe->piece_count;
    for(unsigned i=0;i<recipe->piece_count;++i){
        const auto& r=recipe->pieces[i];CharacterBytes bin,tpl;
        if(!character_archive_entry(archive,r.bin,"BIN",bin)||!character_archive_entry(archive,r.tpl,"TPL",tpl)
            ||!character_asset_detail::tpl(tpl,recipe->texture_pack,&value)
            ||!character_asset_detail::bin(character_asset_detail::mutable_span(bin),recipe->body_bone_count,value.pieces[i])
            ||!character_asset_detail::apply_role(value.pieces[i],r.role)
            ||!character_asset_detail::add_deformed(value,value.pieces[i]))return false;
        value.total_vertex_count+=value.pieces[i].vertex_count;
        value.total_normal_count+=value.pieces[i].normal_count;
        value.pieces[i].tpl=character_asset_detail::mutable_span(tpl);
        value.pieces[i].flags_or=r.flags_or;value.pieces[i].flags_clear=r.flags_clear;
        if(!i&&!character_asset_detail::bind(bin,recipe->body_bone_count,value.body_bind.data()))return false;
    }
    out=value;return true;
}
}
