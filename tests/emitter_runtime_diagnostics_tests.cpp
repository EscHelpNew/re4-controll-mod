#define RE4_EMITTER_FIXTURE_ONLY
#include "character_emitter_module_tests.cpp"
#undef RE4_EMITTER_FIXTURE_ONLY

namespace {
void reset_emitter_trace(){
    emitter_diagnostics.sequence=0;
    for(unsigned channel=0;channel<4;++channel)for(unsigned word=0;word<20;++word)
        emitter_diagnostics.records[channel][word]=0;
}
}
int main(){
    {
        EmitterFixture f;
        // Exercise the actual install/reset order, not the fixture's manually
        // supplied script resolver. Losing this binding disables all mode1
        // flash corrections despite successful model ownership and drawing.
        character_appearance_install();
        assert(character_native.effect_script!=nullptr);
        assert(reinterpret_cast<std::uintptr_t>(character_native.effect_script)==main_base+0x8D470);
    }
    {
        EmitterFixture f;reset_emitter_trace();
        auto* hand=reinterpret_cast<const re4::ViewMatrix3x4*>(f.player_bones[10]+0x18);
        assert(emitter_near(f.point(0x2DAF40,hand),{1,-298,3}));
        assert(emitter_diagnostics.records[1][0]==1&&emitter_diagnostics.records[1][1]==1);
        assert(emitter_diagnostics.records[1][2]==0x2DAF40&&emitter_diagnostics.records[1][19]==0x2DAF40);
        assert(emitter_diagnostics.records[1][18]==unsigned(re4::EmitterDiagnosticResult::Applied));
        assert(emitter_diagnostics.records[1][5]==unsigned(re4::EmitterDiagnosticOwner::Owned));
        assert(emitter_diagnostics.records[1][7]&1u&&emitter_diagnostics.records[1][8]&1u);
        assert(!(emitter_diagnostics.sequence&1));
        // A matrix excluded by the caller policy incurs no diagnostic work.
        const auto sequence=emitter_diagnostics.sequence;
        f.point(0x4EC134);assert(emitter_diagnostics.sequence==sequence);
        // Preserve actual fallback behavior while retaining its precise reason.
        auto changed=f.part;changed.m[0][3]+=1;character_store(f.weapon_part,0x74,changed);
        assert(emitter_near(f.point(0x2DAF40,hand),{1,2,3}));
        assert(emitter_diagnostics.records[1][0]==2&&emitter_diagnostics.records[1][1]==1);
        assert(emitter_diagnostics.records[1][3]==0x2DAF40);
        assert(emitter_diagnostics.records[1][4]==unsigned(re4::EmitterDiagnosticResult::Owner));
        assert(emitter_diagnostics.records[1][5]==unsigned(re4::EmitterDiagnosticOwner::Descriptor));
        assert(emitter_diagnostics.records[1][6]==0x82);
    }
    {
        EmitterFixture f;reset_emitter_trace();
        character_render_lock.test_and_set();
        assert(emitter_near(f.point(0x1F4A48),{1,2,3}));
        character_render_lock.clear();
        assert(emitter_diagnostics.records[2][4]==unsigned(re4::EmitterDiagnosticResult::LockBusy));
        assert(emitter_diagnostics.records[2][0]==1&&emitter_diagnostics.records[2][1]==0);
    }
    {
        EmitterFixture f;reset_emitter_trace();
        assert(emitter_near(f.point(0x2DAF40,f.world()),{1,2,3}));
        assert(emitter_diagnostics.records[1][4]==unsigned(re4::EmitterDiagnosticResult::Matrix));
        assert(emitter_diagnostics.records[1][1]==0);
    }
    {
        EmitterFixture f;reset_emitter_trace();
        // Environmental particles share the hot native transform helpers.
        // A foreign matrix must be rejected before inspecting held geometry
        // or deriving the full26-bone donor pose on every particle update.
        game_test::memory_queries=0;
        assert(emitter_near(f.point(0x1D6A4C,&identity_matrix),{1,2,3}));
        std::printf("foreign emitter memory queries: %u\n",game_test::memory_queries);
        assert(game_test::memory_queries<=6);
        assert(emitter_diagnostics.records[2][1]==0);
    }
    {
        EmitterFixture f;
        alignas(32) std::uint8_t auxiliary[0x450]{},bones[7][0x1F0]{},part[0x1A8]{};
        std::memcpy(auxiliary,f.weapon,sizeof(auxiliary));
        std::memcpy(bones,f.bones,sizeof(bones));std::memcpy(part,f.weapon_part,sizeof(part));
        character_store(auxiliary,0x108,static_cast<void*>(bones));
        character_store(auxiliary,0x180,static_cast<void*>(part));
        character_store(bones[0],0x78,static_cast<void*>(f.player_bones[16]));
        for(unsigned i=0;i<7;++i)character_store(bones[i],0x108,i<6?static_cast<void*>(bones[i+1]):nullptr);
        for(auto region:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(auxiliary),sizeof(auxiliary),1},
            {reinterpret_cast<std::uintptr_t>(bones),sizeof(bones),1},
            {reinterpret_cast<std::uintptr_t>(part),sizeof(part),1}})game_test::memory_maps.push_back(region);
        character_store(f.owner,0x40,static_cast<void*>(auxiliary));
        std::uintptr_t root=0,parent=0,archive=0;unsigned parent_index=3;
        assert(character_held_parent(character_banks[0],reinterpret_cast<std::uintptr_t>(auxiliary),false,1,root,parent,parent_index));
        assert(parent_index==1);
        assert(character_held_archive(character_banks[0],reinterpret_cast<std::uintptr_t>(auxiliary),false,archive));
        auto& bank=character_banks[0];bank.slot_ack[0].store(bank.slot_token[0].load());view_frame.fetch_add(1);
        f.submit();f.draw_weapon();
        assert(character_banks[0].visual[0].held[1].captured);
        CharacterDraw::Callback(auxiliary,part,nullptr,0);assert(drawn_head!=part);
        reset_emitter_trace();
        assert(emitter_near(f.point(0x1F4A48),{1,-298,3}));
        assert(emitter_diagnostics.records[2][0]==1&&emitter_diagnostics.records[2][1]==1);
        assert(emitter_diagnostics.records[2][9]==0); // Primary at BODY10, auxiliary at BODY16.
        float visual_x=0,visual_y=0;
        const auto x_bits=emitter_diagnostics.records[2][12],y_bits=emitter_diagnostics.records[2][13];
        std::memcpy(&visual_x,&x_bits,sizeof(visual_x));std::memcpy(&visual_y,&y_bits,sizeof(visual_y));
        assert(std::fabs(visual_x)<.002&&std::fabs(visual_y+300)<.002);
        auto* hand=reinterpret_cast<const re4::ViewMatrix3x4*>(f.player_bones[10]+0x18);
        assert(emitter_near(f.point(0x2DAF40,hand),{1,-298,3}));
        assert(emitter_diagnostics.records[1][0]==1&&emitter_diagnostics.records[1][1]==1);
        assert(emitter_diagnostics.records[1][9]==0); // BODY10 producer overrides held parent16.
        const auto body_x_bits=emitter_diagnostics.records[1][12];
        std::memcpy(&visual_x,&body_x_bits,sizeof(visual_x));assert(std::fabs(visual_x)<.002);
        auto changed=f.part;changed.m[0][3]+=1;character_store(f.weapon_part,0x74,changed);
        assert(emitter_near(f.point(0x1F4A48),{1,2,3}));
        assert(emitter_diagnostics.records[2][4]==unsigned(re4::EmitterDiagnosticResult::Owner));
        assert(emitter_diagnostics.records[2][5]==unsigned(re4::EmitterDiagnosticOwner::Descriptor));
        assert(emitter_diagnostics.records[2][6]==0x82&&emitter_diagnostics.records[2][9]==0);
    }
    {
        EmitterFixture f;reset_emitter_trace();
        character_banks[0].visual[0].held[0].drawn=false;
        assert(emitter_near(f.point(0x1F4A48),{1,2,3}));
        assert(emitter_diagnostics.records[2][4]==unsigned(re4::EmitterDiagnosticResult::Owner));
        assert(emitter_diagnostics.records[2][5]==unsigned(re4::EmitterDiagnosticOwner::Frame));
        assert(emitter_diagnostics.records[2][7]==1&&emitter_diagnostics.records[2][8]==0);
    }
    {
        EmitterFixture f;reset_emitter_trace();
        auto bad=identity_matrix;bad.m[0][0]=std::numeric_limits<float>::quiet_NaN();
        character_store(f.player_bones[4],0x18,bad);
        assert(emitter_near(f.point(0x1F4A48),{1,2,3}));
        assert(emitter_diagnostics.records[2][4]==unsigned(re4::EmitterDiagnosticResult::Pose));
    }
    {
        EmitterFixture f;reset_emitter_trace();
        // Diagnostics contention must neither block nor disable correction.
        character_emitter_observer_lock.test_and_set();
        assert(emitter_near(f.point(0x1F4A48),{1,-298,3}));
        assert(emitter_diagnostics.sequence==0&&emitter_diagnostics.records[2][0]==0);
        character_emitter_observer_lock.clear();
        assert(emitter_near(f.point(0x1F4A48),{1,-298,3}));
        assert(emitter_diagnostics.records[2][0]==1&&emitter_diagnostics.records[2][1]==1);
    }
    for(unsigned invalid=0;invalid<9;++invalid){
        EmitterFixture f;reset_emitter_trace();re4::ViewMatrix3x4 out{};
        // Both held handles may be populated in the real game. A mismatch
        // against the auxiliary actor must not obscure the primary's reason.
        if(invalid!=2)character_store(f.owner,0x40,static_cast<void*>(f.body));
        if(invalid==0)f.effects[0][0x18]=0;
        if(invalid==1)f.effects[0][0x19]=0x11;
        if(invalid==2)character_store(f.effects[0],0x28,static_cast<void*>(f.body));
        if(invalid==3)character_store(f.effects[0],0x30,1u);
        if(invalid==4)character_store(f.effects[0],0x38,std::uint16_t(0));
        if(invalid==5)f.effects[0][3]=0x37;
        if(invalid==6)f.effects[0][0x10]=2;
        if(invalid==7)character_store(f.effects[0],0x20,emitter_script+4);
        if(invalid==8)character_store(f.f.image.data(),0x113CC00,0u);
        const unsigned reasons[]={9,9,10,11,12,13,13,14,8};
        character_emitter_caller=main_base+0x1E81F0;
        CharacterEmitterCopy::Callback(reinterpret_cast<const re4::ViewMatrix3x4*>(f.effects[0]+0x40),&out);
        assert(out.m[1][3]==0&&character_emitter_copy_calls==1);
        assert(emitter_diagnostics.records[3][0]==1&&emitter_diagnostics.records[3][1]==0);
        assert(emitter_diagnostics.records[3][4]==reasons[invalid]);
    }
}
