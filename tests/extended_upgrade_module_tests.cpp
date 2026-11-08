// Actual extended-upgrade callbacks against a bounded fake image: tune-up
// purchase, damage scaling, pruning, checkpoint rollback and save sidecar.
#define RE4_EXTENDED_SQUARE_TEST_NATIVE
namespace {void test_ExtendedSquareDraw(void*);}
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
std::array<std::array<std::uint8_t,0x170>,0xA0> square_sprites{};
std::vector<std::array<std::uint8_t,0x170>> square_draws;
void test_ExtendedSquareDraw(void* sprite){
    std::array<std::uint8_t,0x170> bytes{};
    if(sprite)std::memcpy(bytes.data(),sprite,bytes.size());
    square_draws.push_back(bytes);
}
void* square_sprite(void*,unsigned id,unsigned group){
    return group==0x1C&&id<square_sprites.size()?square_sprites[id].data():nullptr;
}
struct UpgradeFixture {
    Fixture f;
    alignas(16) std::array<std::uint8_t,0x90> state{};
    alignas(16) std::array<std::uint8_t,14*4> slots{};
    alignas(16) std::array<std::uint8_t,0x60> work{};    // TuneWork
    alignas(16) std::array<std::uint8_t,0x40> confirm{}; // confirm state +20..23
    alignas(16) std::array<std::uint8_t,0x500> save{};   // save menu ctx
    std::vector<std::uint8_t> slot_work=std::vector<std::uint8_t>(0xEB00);
    exl::hook::InlineFloatCtx context{};
    template<class T>void put(void* p,std::size_t offset,T value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));}
    template<class T>T get(const void* p,std::size_t offset){T v;std::memcpy(&v,static_cast<const std::uint8_t*>(p)+offset,sizeof(v));return v;}
    std::uintptr_t address(const void* p){return reinterpret_cast<std::uintptr_t>(p);}
    std::uint8_t* record(unsigned i){return slots.data()+i*14;}
    void reg(unsigned i,std::uintptr_t v){re4::set_saved_x(context.bytes,i,v);}
    UpgradeFixture(){
        settings.extended_weapon_upgrade=true;
        extended_upgrade_mask=0;extended_upgrade_pending={};extended_upgrade_snapshots={};
        extended_upgrade_store={};extended_upgrade_store_loaded=false;extended_upgrade_square={};
        extended_upgrade_store_dirty=false;
        extended_upgrade_mask_known=true;extended_upgrade_loaded_identity={};
        game_test::extended_upgrade_file.clear();game_test::extended_upgrade_writes=0;
        game_test::extended_upgrade_file_exists=false;
        game_test::extended_upgrade_open_read_result=game_test::extended_upgrade_open_write_result=0;
        game_test::extended_upgrade_size_result=game_test::extended_upgrade_read_result=0;
        game_test::extended_upgrade_create_result=game_test::extended_upgrade_write_result=0;
        put(f.image.data(),0xB1AE08,address(state.data()));
        const auto inv=f.image.data()+kExtendedInventory;
        put(inv,0x20,address(slots.data()));put(inv,0x30,std::int32_t(4));inv[0x1B]=0;
        // Handgun at Firepower nibble5 (native max 6), Red9 at nibble2, shells.
        put(record(0),0,std::uint16_t(0x23));put(record(0),2,std::uint16_t(1));record(0)[4]=1;put(record(0),6,std::uint32_t(0x00A80005));
        put(record(1),0,std::uint16_t(0x25));put(record(1),2,std::uint16_t(1));record(1)[4]=1;put(record(1),6,std::uint32_t(0x2));
        put(record(2),0,std::uint16_t(0x18));put(record(2),2,std::uint16_t(30));record(2)[4]=1;
        put(inv,0x10,address(record(0)));
        put(work.data(),0x28,address(record(0)));put(work.data(),0x34,std::int32_t(0));
        for(unsigned i=0;i<4;++i)confirm[0x20+i]=std::uint8_t(i==0?6:1);
        put(save.data(),0x4D8,address(slot_work.data()));save[0x1D]=3;put(slot_work.data(),0xEAF8,std::uint32_t(0xC0FFEE));
        game_test::memory_maps={{main_base,f.image.size(),3},{address(state.data()),state.size(),3},
            {address(slots.data()),slots.size(),3},{address(work.data()),work.size(),3},
            {address(confirm.data()),confirm.size(),3},{address(save.data()),save.size(),3},
            {address(slot_work.data()),slot_work.size(),3}};
        file_mounted=true;
    }
    void row_price(unsigned row=0){
        put(work.data(),0x34,std::int32_t(row));put(work.data(),0x30,std::int32_t(0));
        context={};reg(25,address(work.data()));reg(22,address(record(0)));reg(24,6);reg(0,6);
        ExtendedRowPrice::Callback(&context);
    }
    void confirm_enter(){
        for(unsigned i=0;i<4;++i)put(work.data(),0x38+i*4,std::int32_t(9)); // native "+1" / limit writes
        context={};reg(22,address(work.data()));reg(20,address(confirm.data()));
        ExtendedConfirmEnter::Callback(&context);
    }
    void commit(){
        alignas(16) static std::array<std::uint8_t,0x400> owner{};
        put(owner.data(),0x3F8,address(work.data()));
        game_test::memory_maps.push_back({address(owner.data()),owner.size(),3});
        context={};reg(20,address(owner.data()));ExtendedCommit::Callback(&context);
        game_test::memory_maps.pop_back();
    }
    float damage(unsigned weapon_index,float value){
        context={};reg(20,weapon_index);re4::set_saved_s0(context.bytes,value);
        ExtendedDamage::Callback(&context);float out;std::memcpy(&out,context.bytes,4);return out;
    }
    void buy(){row_price();confirm_enter();commit();}
    void save_slot(){context={};reg(19,address(save.data()));ExtendedSave::Callback(&context);}
    void load_slot(){context={};reg(19,address(save.data()));ExtendedLoad::Callback(&context);}
    void disk(bool current=true){
        re4::ExtendedUpgradeStore store;
        re4::extended_upgrade_put(store,9,0x12345678,2);
        if(current)re4::extended_upgrade_put(store,3,0xC0FFEE,1);
        std::uint8_t bytes[re4::kExtendedUpgradeFileBytes];re4::extended_upgrade_encode(store,bytes);
        game_test::extended_upgrade_file.assign(bytes,bytes+sizeof(bytes));game_test::extended_upgrade_file_exists=true;
    }
    void assert_other_slot(){
        re4::ExtendedUpgradeStore store;std::uint16_t mask=0;
        assert(re4::extended_upgrade_decode(game_test::extended_upgrade_file.data(),game_test::extended_upgrade_file.size(),store));
        assert(re4::extended_upgrade_get(store,9,0x12345678,mask)&&mask==2);
    }
};
void square_visibility_test(){
    UpgradeFixture h;square_sprites={};health_color_probe=true;
    game_test::memory_maps.push_back({h.address(square_sprites.data()),sizeof(square_sprites),3});
    extended_upgrade_native.sprite=square_sprite;
    h.put(h.f.image.data(),0xB26C90,h.address(square_sprites.data()));
    for(unsigned i=0;i<6;++i){
        auto& s=square_sprites[0x65+i];s[0]=9;
        h.put(s.data(),0x14,100.f+20.f*float(i));h.put(s.data(),0x24,-50.f);
        const float corners[4][3]={{-8,6,0},{8,6,0},{8,-6,0},{-8,-6,0}};
        std::memcpy(s.data()+0xBC,corners,sizeof(corners));
    }
    const std::uint8_t gray[4]={91,92,93,160};
    std::memcpy(square_sprites[3].data()+0xFC,gray,sizeof(gray));
    ExtendedLevelBar::Callback(h.record(0),0x23,1);
    ExtendedLevelBar::Callback(h.record(0),0x23,0);
    draw_extended_upgrade_hud();
    // A native hide must remove the extra immediately, even within the old
    // overlay's 50ms cache window.
    assert(gx_vertices==0);
}
struct SquareFixture : UpgradeFixture {
    alignas(16) std::array<std::uint8_t,0x170> parent{};
    SquareFixture(){
        square_sprites={};square_draws.clear();extended_upgrade_native.sprite=square_sprite;
        game_test::memory_maps.push_back({address(square_sprites.data()),sizeof(square_sprites),3});
        game_test::memory_maps.push_back({address(parent.data()),parent.size(),3});
        put(f.image.data(),0xB26C90,address(square_sprites.data()));
        const std::uint8_t red[4]={201,31,23,255},gray[4]={91,92,93,255};
        std::memcpy(square_sprites[2].data()+0xFC,red,sizeof(red));
        std::memcpy(square_sprites[3].data()+0xFC,gray,sizeof(gray));
        const float tint[4]={128,255,64,160};std::memcpy(parent.data()+0x104,tint,sizeof(tint));
        for(unsigned i=0;i<6;++i){
            auto& s=square_sprites[0x65+i];s[0]=9;s[0x88]=7;s[0x8A]=11;s[0x9E]=0;
            const float matrix[3][4]={{.8f,.1f,0,100.f+20.f*float(i)},{.2f,.9f,0,-50},{0,0,1,3}};
            const float corners[4][3]={{-8,6,0},{8,6,0},{8,-6,0},{-8,-6,0}};
            const float rgba[4]={100,31,5,77};
            std::memcpy(s.data()+8,matrix,sizeof(matrix));
            std::memcpy(s.data()+0xBC,corners,sizeof(corners));
            std::memcpy(s.data()+0x104,rgba,sizeof(rgba));
            put(s.data(),0x68,address(parent.data()));
        }
    }
    void select(void* selected,unsigned raw,unsigned show=1){ExtendedLevelBar::Callback(selected,raw,show);square_draws.clear();}
    void draw(unsigned id=0x6A){square_draws.clear();ExtendedSquareDraw::Callback(square_sprites[id].data());}
    void color(float r,float g,float b,float a){
        assert(square_draws.size()==2);
        const auto* copy=square_draws[1].data();
        assert(get<float>(copy,0x104)==r&&get<float>(copy,0x108)==g&&get<float>(copy,0x10C)==b&&get<float>(copy,0x110)==a);
    }
};
void native_square_tests(){
    {
        SquareFixture h;h.select(h.record(0),0x23);
        // Native transforms are evaluated after LevelBar. The extra must use
        // these current matrices and retain every quad/texture/style byte.
        h.put(square_sprites[0x69].data(),0x14,177.f);h.put(square_sprites[0x6A].data(),0x14,197.f);
        h.put(square_sprites[0x69].data(),0x24,-49.f);h.put(square_sprites[0x6A].data(),0x24,-48.f);
        h.put(square_sprites[0x69].data(),0x34,2.f);
        const auto unchanged=square_sprites;
        h.draw();h.color(45,92,23,77);
        const auto& copy=square_draws[1];
        assert(h.get<float>(copy.data(),0x14)==217&&h.get<float>(copy.data(),0x24)==-47&&h.get<float>(copy.data(),0x34)==4);
        assert(!std::memcmp(copy.data()+0xBC,square_sprites[0x6A].data()+0xBC,48));
        assert(copy[0x88]==7&&copy[0x8A]==11&&copy[0x9E]==0);
        assert(h.get<float>(copy.data(),8)==.8f&&h.get<float>(copy.data(),0xC)==.1f);
        assert(square_sprites==unchanged); // Never overwrite any native row.
        h.buy();h.select(h.record(0),0x23);h.draw();h.color(100,31,5,77);
        h.put(square_sprites[0x6A].data(),0x110,0.f);h.draw();h.color(100,31,5,0);
        h.put(square_sprites[0x6A].data(),0x110,19.f);h.draw();h.color(100,31,5,19);
        // Other native rows and option-off cannot produce the extra.
        h.draw(0x75);assert(square_draws.size()==1);
        settings.extended_weapon_upgrade=false;h.draw();assert(square_draws.size()==1);
        settings.extended_weapon_upgrade=true;h.state[0x79]=0x10;h.draw();assert(square_draws.size()==1);h.state[0x79]=0;
        // Hide and unsupported raw id invalidate the capture immediately.
        h.select(h.record(0),0x23,0);h.draw();assert(square_draws.size()==1);
        h.select(nullptr,0x18);h.draw();assert(square_draws.size()==1);
    }
    {
        SquareFixture h;extended_upgrade_mask=1;
        // Selecting a lower-level duplicate must not inherit the equipped
        // duplicate's owned extra. Neither may a stock preview with no record.
        h.put(h.record(3),0,std::uint16_t(0x23));h.put(h.record(3),2,std::uint16_t(1));h.record(3)[4]=1;
        h.put(h.record(3),6,std::uint32_t(0));h.select(h.record(3),0x23);h.draw();h.color(45,92,23,77);
        h.select(h.record(0),0x23);h.draw();h.color(100,31,5,77);
        h.select(nullptr,0x23);h.draw();h.color(45,92,23,77);
        // The exact selected record is checked again at the draw boundary.
        h.select(h.record(0),0x23);h.record(0)[4]=0;h.draw();assert(square_draws.size()==1);
        h.record(0)[4]=1;h.select(h.record(0),0x25);h.draw();assert(square_draws.size()==1);
    }
    {
        SquareFixture h;
        // Killer7's native maximum is three: the fourth square is placed one
        // pitch after 0x67, never after the sixth native sprite.
        h.put(h.record(0),0,std::uint16_t(0x2A));h.put(h.record(0),6,std::uint32_t(2));
        h.select(h.record(0),0x2A);h.draw(0x67);h.color(45,92,23,77);
        assert(h.get<float>(square_draws[1].data(),0x14)==160);
        h.draw();assert(square_draws.size()==1);
        extended_upgrade_mask=std::uint16_t(1u<<12);h.draw(0x67);h.color(100,31,5,77);
    }
    for(unsigned fault=0;fault<8;++fault){
        SquareFixture h;h.select(h.record(0),0x23);
        if(fault==0)square_sprites[0x6A][0]=1;
        if(fault==1)square_sprites[0x69][0]=0xFF;
        if(fault==2)h.put(square_sprites[0x6A].data(),0x14,NAN);
        if(fault==3)h.put(square_sprites[0x6A].data(),0x14,180.f); // Zero pitch.
        if(fault==4)h.put(square_sprites[0x6A].data(),0x110,NAN);
        if(fault==5)h.put(h.parent.data(),0x104,256.f);
        if(fault==6)h.put(square_sprites[0x6A].data(),0x68,std::uintptr_t(0x123));
        if(fault==7)square_sprites[0x6A][0x9E]=1; // Unproved specialized renderer path.
        h.draw();assert(square_draws.size()==1);
    }
}
void persistence_fault_tests(){
    for(unsigned fault=0;fault<5;++fault){
        UpgradeFixture h;h.disk(false);h.buy();const auto original=game_test::extended_upgrade_file;
        if(fault==0)game_test::extended_upgrade_open_read_result=7;
        if(fault==1)game_test::extended_upgrade_size_result=7;
        if(fault==2)game_test::extended_upgrade_read_result=7;
        if(fault==3)game_test::extended_upgrade_file[8]^=1; // Checksum corruption.
        if(fault==4)game_test::extended_upgrade_file.resize(3); // Invalid size.
        const auto unread=game_test::extended_upgrade_file;
        h.save_slot();assert(game_test::extended_upgrade_file==unread&&!game_test::extended_upgrade_writes);
        assert(!extended_upgrade_store_loaded);
        game_test::extended_upgrade_open_read_result=game_test::extended_upgrade_size_result=game_test::extended_upgrade_read_result=0;
        game_test::extended_upgrade_file=original;
        h.save_slot();assert(game_test::extended_upgrade_writes==1);h.assert_other_slot();
    }
    for(unsigned fault=0;fault<2;++fault){
        UpgradeFixture h;h.disk(false);h.buy();const auto original=game_test::extended_upgrade_file;
        if(fault==0)game_test::extended_upgrade_open_write_result=7;
        else game_test::extended_upgrade_write_result=7;
        h.save_slot();assert(game_test::extended_upgrade_file==original);
        std::uint16_t mask=0;assert(!re4::extended_upgrade_get(extended_upgrade_store,3,0xC0FFEE,mask));
        game_test::extended_upgrade_open_write_result=game_test::extended_upgrade_write_result=0;
        h.save_slot();h.assert_other_slot();
        assert(re4::extended_upgrade_get(extended_upgrade_store,3,0xC0FFEE,mask)&&mask==1);
        assert(game_test::extended_upgrade_writes==(fault==0?1u:2u));
    }
    for(unsigned fault=0;fault<5;++fault){
        UpgradeFixture h;h.disk();const auto original=game_test::extended_upgrade_file;
        if(fault==0)game_test::extended_upgrade_open_read_result=7;
        if(fault==1)game_test::extended_upgrade_size_result=7;
        if(fault==2)game_test::extended_upgrade_read_result=7;
        if(fault==3)game_test::extended_upgrade_file[8]^=1;
        if(fault==4)game_test::extended_upgrade_file.resize(3);
        const auto unread=game_test::extended_upgrade_file;
        h.load_slot();assert(!extended_upgrade_mask);
        h.row_price();assert(!extended_upgrade_pending.work); // Unknown ownership cannot be sold twice.
        h.save_slot();assert(!game_test::extended_upgrade_writes&&game_test::extended_upgrade_file==unread);
        alignas(16) std::uint8_t checkpoint[16]{};ExtendedSnapshotTake::Callback(checkpoint,0,0,0);
        game_test::extended_upgrade_open_read_result=game_test::extended_upgrade_size_result=game_test::extended_upgrade_read_result=0;
        game_test::extended_upgrade_file=original;
        h.put(h.slot_work.data(),0xEAF8,std::uint32_t(0xBEEFFEED)); // New native save CRC.
        settings.extended_weapon_upgrade=false;h.save_slot();h.assert_other_slot();
        assert(extended_upgrade_mask==1);
        re4::ExtendedUpgradeStore persisted;std::uint16_t mask=0;
        assert(re4::extended_upgrade_decode(game_test::extended_upgrade_file.data(),game_test::extended_upgrade_file.size(),persisted));
        assert(re4::extended_upgrade_get(persisted,3,0xBEEFFEED,mask)&&mask==1);
        ExtendedSnapshotRestore::Callback(checkpoint,0,0,0);assert(extended_upgrade_mask==1);
        settings.extended_weapon_upgrade=true;h.row_price();assert(!extended_upgrade_pending.work);
    }
    {
        UpgradeFixture h;h.buy();game_test::extended_upgrade_create_result=7;h.save_slot();
        assert(game_test::extended_upgrade_file.empty()&&!game_test::extended_upgrade_writes);
        game_test::extended_upgrade_create_result=0;h.save_slot();assert(game_test::extended_upgrade_writes==1);
    }
    {
        UpgradeFixture h;h.disk();settings.extended_weapon_upgrade=false;h.load_slot();h.save_slot();
        assert(extended_upgrade_mask==1&&!game_test::extended_upgrade_writes);h.assert_other_slot();
    }
    {
        UpgradeFixture h;h.buy();h.save_slot();assert(game_test::extended_upgrade_writes==1);
        h.record(0)[4]=0;game_test::extended_upgrade_write_result=7;h.save_slot();
        assert(game_test::extended_upgrade_writes==2); // A failed write may have reached the medium partially.
        h.record(0)[4]=1;extended_upgrade_mask=1;game_test::extended_upgrade_write_result=0;
        h.save_slot();assert(game_test::extended_upgrade_writes==3); // Retry even if current mask matches durable cache again.
    }
}
}

int main(){
    square_visibility_test();
    native_square_tests();
    persistence_fault_tests();
    {
        UpgradeFixture h;
        // Purchase: chapter limit/max registers are relaxed only for the extra.
        h.row_price();
        assert(h.get<std::int32_t>(h.work.data(),0x30)==20000);
        assert(re4::saved_x(h.context.bytes,24)==0x7F&&re4::saved_x(h.context.bytes,0)==0xFF);
        assert(extended_upgrade_pending.work==h.address(h.work.data()));
        // Damage is unchanged before the purchase completes.
        assert(near(h.damage(2,2.f),2.f));
        h.confirm_enter();
        for(unsigned i=0;i<4;++i)assert(h.get<std::int32_t>(h.work.data(),0x38+i*4)==(i==0?6:1)); // nibble stays native
        h.commit();
        assert(extended_upgrade_mask==1u&&!extended_upgrade_pending.work);
        // Damage: equipped handgun index2 scales by 2.2/2.0; other indices do not.
        assert(near(h.damage(2,2.f),2.2f)&&near(h.damage(3,3.7f),3.7f));
        // Exclusive owned (nibble6): the same ratio on top of the native value.
        h.put(h.record(0),6,std::uint32_t(0x00A80006));assert(near(h.damage(2,2.f),2.2f));
        // Disabled option or preset mode: native values.
        settings.extended_weapon_upgrade=false;assert(near(h.damage(2,2.f),2.f));settings.extended_weapon_upgrade=true;
        h.state[0x79]=0x10;assert(near(h.damage(2,2.f),2.f));h.state[0x79]=0;
        // Owned extra cannot be bought twice.
        h.row_price();assert(re4::saved_x(h.context.bytes,24)==6&&!extended_upgrade_pending.work);
    }
    {
        UpgradeFixture h;
        // Not at the native maximum: the native path is untouched.
        h.put(h.record(0),6,std::uint32_t(0x00A80004));h.row_price();
        assert(h.get<std::int32_t>(h.work.data(),0x30)==0&&re4::saved_x(h.context.bytes,24)==6&&!extended_upgrade_pending.work);
        // Other rows, other records, disabled option: untouched.
        h.put(h.record(0),6,std::uint32_t(0x00A80005));
        h.row_price(1);assert(!extended_upgrade_pending.work&&re4::saved_x(h.context.bytes,0)==6);
        settings.extended_weapon_upgrade=false;h.row_price();assert(!extended_upgrade_pending.work);settings.extended_weapon_upgrade=true;
        h.state[0x79]=0x10;h.row_price();assert(!extended_upgrade_pending.work);h.state[0x79]=0;
        // Confirm/commit without a matching pending purchase never grant it.
        h.confirm_enter();assert(h.get<std::int32_t>(h.work.data(),0x38)==9);
        h.commit();assert(!extended_upgrade_mask);
        // A pending extra followed by a different row (exclusive) is not reset.
        h.row_price();h.put(h.work.data(),0x34,std::int32_t(4));h.confirm_enter();
        assert(h.get<std::int32_t>(h.work.data(),0x38)==9&&!extended_upgrade_pending.work);
        h.commit();assert(!extended_upgrade_mask);
    }
    {
        UpgradeFixture h;
        // Fully tuned weapons reach the per-row path only for row0 and a buyable extra.
        h.context={};h.reg(0,1);h.reg(25,h.address(h.work.data()));ExtendedFullyTuned::Callback(&h.context);
        assert(re4::saved_x(h.context.bytes,0)==0);
        h.put(h.work.data(),0x34,std::int32_t(4));h.context={};h.reg(0,1);h.reg(25,h.address(h.work.data()));
        ExtendedFullyTuned::Callback(&h.context);assert(re4::saved_x(h.context.bytes,0)==1);
        // Can-tune: native false becomes true only while the extra is buyable.
        assert(ExtendedCanTune::Callback(nullptr,h.record(0))==1&&ExtendedCanTune::Callback(nullptr,h.record(1))==0);
        h.buy();assert(extended_upgrade_mask==1&&ExtendedCanTune::Callback(nullptr,h.record(0))==0);
    }
    {
        UpgradeFixture h;h.buy();
        // Checkpoint: take, buy-state change, restore rolls the mask back.
        alignas(16) static std::uint8_t checkpoint[16];
        ExtendedSnapshotTake::Callback(checkpoint,0,0,0);
        extended_upgrade_mask=0;ExtendedSnapshotRestore::Callback(checkpoint,0,0,0);assert(extended_upgrade_mask==1);
        // Save writes the sidecar for slot3/CRC; loading another CRC clears it.
        h.context={};h.reg(19,h.address(h.save.data()));ExtendedSave::Callback(&h.context);
        assert(game_test::extended_upgrade_writes==1&&game_test::extended_upgrade_file.size()==re4::kExtendedUpgradeFileBytes);
        ExtendedSave::Callback(&h.context);assert(game_test::extended_upgrade_writes==1); // unchanged: no rewrite
        extended_upgrade_mask=0;extended_upgrade_store={};extended_upgrade_store_loaded=false;
        h.context={};h.reg(19,h.address(h.save.data()));ExtendedLoad::Callback(&h.context);assert(extended_upgrade_mask==1);
        h.put(h.slot_work.data(),0xEAF8,std::uint32_t(0xBADF00D));ExtendedLoad::Callback(&h.context);assert(extended_upgrade_mask==0);
        h.put(h.slot_work.data(),0xEAF8,std::uint32_t(0xC0FFEE));h.save[0x1D]=4;ExtendedLoad::Callback(&h.context);assert(extended_upgrade_mask==0);
        h.save[0x1D]=3;ExtendedLoad::Callback(&h.context);assert(extended_upgrade_mask==1);
        // Selling the weapon (record cleared) prunes ownership at the next checkpoint.
        h.record(0)[4]=0;ExtendedSnapshotTake::Callback(checkpoint,0,0,0);assert(extended_upgrade_mask==0);
        // New Game: a fresh low-level handgun also prunes it.
        extended_upgrade_mask=1;h.record(0)[4]=1;h.put(h.record(0),6,std::uint32_t(0));
        ExtendedSnapshotTake::Callback(checkpoint,0,0,0);assert(extended_upgrade_mask==0);
    }
    {
        UpgradeFixture h;h.buy();
        // Attache hover readout follows the damage ratio.
        assert(near(extended_upgrade_display_ratio(h.address(h.record(0))),1.1f)&&near(extended_upgrade_display_ratio(h.address(h.record(1))),1.f));
    }
    std::puts("actual extended upgrade callbacks: native extra square, purchase, damage, prune, checkpoint and sidecar PASS");
}
