#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
struct MoneyFixture {
    Fixture f;
    alignas(16) std::array<std::uint8_t,0x500> save{};
    alignas(16) std::array<std::uint8_t,0x100> title{};
    std::vector<std::uint8_t> work=std::vector<std::uint8_t>(0xEB00);
    exl::hook::InlineFloatCtx ctx{};
    template<class T>void put(void* p,std::size_t at,T value){std::memcpy(static_cast<std::uint8_t*>(p)+at,&value,sizeof(value));}
    template<class T>T get(const void* p,std::size_t at){T value{};std::memcpy(&value,static_cast<const std::uint8_t*>(p)+at,sizeof(value));return value;}
    std::uintptr_t address(const void* p){return reinterpret_cast<std::uintptr_t>(p);}
    MoneyFixture(){
        money_reset();game_test::money_files={};game_test::money_exists={};game_test::money_write_result=0;
        game_test::money_read_result=game_test::money_open_result=0;game_test::money_writes=0;
        f.view_mode();put(f.image.data(),0xAFF39C,std::int32_t(2));put(f.input_state.data(),0x501C,std::int32_t(1234));
        put(save.data(),0x4D8,address(work.data()));save[0x1D]=3;put(work.data(),0xEAF8,std::uint32_t(0xC0FFEE));
        game_test::memory_maps={{main_base,f.image.size(),3},{address(f.actor),sizeof(f.actor),3},
            {address(f.input_state.data()),f.input_state.size(),3},{address(save.data()),save.size(),3},
            {address(work.data()),work.size(),3},{address(title.data()),title.size(),3}};
        file_mounted=true;extended_upgrade_store_loaded=false;extended_upgrade_mask_known=true;
        extended_upgrade_loaded_identity={};extended_upgrade_mask=0;
    }
    std::int32_t wallet(){return get<std::int32_t>(f.input_state.data(),0x501C);}
    void load(unsigned slot=3,std::uint32_t crc=0xC0FFEE){save[0x1D]=std::uint8_t(slot);put(work.data(),0xEAF8,crc);ctx={};re4::set_saved_x(ctx.bytes,19,address(save.data()));ExtendedLoad::Callback(&ctx);}
    void save_as(unsigned slot,std::uint32_t crc){save[0x1D]=std::uint8_t(slot);put(work.data(),0xEAF8,crc);ctx={};re4::set_saved_x(ctx.bytes,19,address(save.data()));ExtendedSave::Callback(&ctx);}
    void start(bool plus=false){
        title[0]=7;put(f.input_state.data(),0x78,std::uint32_t(plus?0x100:0));f.input_state[0x84F4]=plus?3:0;
        ctx={};re4::set_saved_x(ctx.bytes,8,address(f.input_state.data()));re4::set_saved_x(ctx.bytes,9,plus?0x100:0);
        re4::set_saved_x(ctx.bytes,19,address(title.data()));MoneyCampaignStart::Callback(&ctx);
    }
    void request(){control.money_request=control.money_request+1;PadNormalize::Callback(nullptr);}
};
}
int main(){
    {
        MoneyFixture h;h.load();h.request();assert(h.wallet()==201234&&control.money_result==1);
        assert(!settings.extended_weapon_upgrade&&control.money_ack==control.money_request);
        h.request();assert(h.wallet()==201234&&control.money_result==2);
        h.save_as(8,0xBEEF);money_reset();h.load(8,0xBEEF);h.request();assert(h.wallet()==201234&&control.money_result==2);
        money_reset();h.load();h.request();assert(h.wallet()==201234&&control.money_result==2);
        h.load(5,0xBAD);h.request();assert(h.wallet()==201234&&control.money_result==3); // Unknown old save cannot bypass claim.
        h.start();h.request();assert(h.wallet()==401234&&control.money_result==1);
        h.save_as(2,0xCAFE);h.load();h.request();assert(h.wallet()==401234&&control.money_result==2);
        h.start(true);h.request();assert(h.wallet()==601234&&control.money_result==1);
    }
    {
        MoneyFixture h;h.load();game_test::money_write_result=9;h.request();assert(h.wallet()==1234&&control.money_result==4);
        game_test::money_write_result=0;h.request();assert(h.wallet()==201234&&control.money_result==1);
        const auto writes=game_test::money_writes;for(unsigned i=0;i<120;++i)PadNormalize::Callback(nullptr);
        assert(game_test::money_writes==writes&&h.wallet()==201234);
    }
    {
        MoneyFixture h;h.load();h.put(h.f.input_state.data(),0x501C,std::int32_t(99799999));h.request();assert(h.wallet()==99999999);
        h.start();h.request();assert(h.wallet()==99999999&&control.money_result==5);
        h.put(h.f.input_state.data(),0x501C,std::int32_t(0));h.request();assert(h.wallet()==200000);
    }
    for(unsigned mode:{0u,0x1000u,0x4000u,0x800000u,0x40000000u,0x80000000u}){
        MoneyFixture h;h.load();
        if(!mode)h.put(h.f.image.data(),0xAFF39C,std::int32_t(0));else h.put(h.f.input_state.data(),0x78,std::uint32_t(mode));
        h.request();assert(h.wallet()==1234&&control.money_result==3&&game_test::money_writes==0);
    }
    {
        MoneyFixture h;h.load();h.request();money_reset();
        for(auto& file:game_test::money_files)if(!file.empty()){file[100]^=1;break;}
        h.load();h.request();assert(h.wallet()==201234&&control.money_result==4);
    }
    {
        MoneyFixture h;h.load();control.money_request=1;h.load();PadNormalize::Callback(nullptr);
        assert(h.wallet()==1234&&control.money_ack==1&&game_test::money_writes==0);
    }
    {
        MoneyFixture h;h.put(h.f.input_state.data(),0x78,std::uint32_t(0x1000));h.load();h.save_as(3,0xBAD);
        assert(game_test::money_writes==0); // Side stories cannot consume onboarding.
        h.put(h.f.input_state.data(),0x78,std::uint32_t(0));h.load(7,0xF00D);h.request();assert(h.wallet()==201234);
    }
    {
        MoneyFixture h;h.load();h.save_as(6,0xABCD);money_reset();h.load(6,0xABCD);h.request();assert(h.wallet()==201234);
        h.put(h.f.input_state.data(),0x78,std::uint32_t(0x100));h.f.input_state[0x84F4]=1;h.title[0]=7;
        h.ctx={};re4::set_saved_x(h.ctx.bytes,8,h.address(h.f.input_state.data()));re4::set_saved_x(h.ctx.bytes,9,0x100);
        re4::set_saved_x(h.ctx.bytes,19,h.address(h.title.data()));MoneyCampaignStart::Callback(&h.ctx);
        h.request();assert(h.wallet()==201234&&control.money_result==2); // Ordinary title continue is not New Game.
        ExtendedSnapshotRestore::Callback(h.work.data(),0,0,0);h.request();assert(h.wallet()==201234);
        MoneyCampaignStart::Callback(nullptr);
    }
    for(int invalid:{-1,100000000}){
        MoneyFixture h;h.load();h.put(h.f.input_state.data(),0x501C,std::int32_t(invalid));h.request();
        assert(h.wallet()==invalid&&control.money_result==3&&game_test::money_writes==0);
    }
    {
        MoneyFixture h;h.load();game_test::memory_maps[2].perm=1;h.request();assert(h.wallet()==1234&&control.money_result==3);
        game_test::memory_maps[2].perm=3;h.f.actor[0x114]=1;h.request();assert(h.wallet()==1234&&control.money_result==3);
    }
    std::puts("actual money bonus callbacks: persistence, restarts, modes, capacity, I/O and stale requests PASS");
}
