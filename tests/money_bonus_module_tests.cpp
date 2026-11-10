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
    std::vector<std::uint8_t> work=std::vector<std::uint8_t>(0xEB00);
    exl::hook::InlineFloatCtx ctx{};
    template<class T>void put(void* p,std::size_t at,T value){std::memcpy(static_cast<std::uint8_t*>(p)+at,&value,sizeof(value));}
    template<class T>T get(const void* p,std::size_t at){T value{};std::memcpy(&value,static_cast<const std::uint8_t*>(p)+at,sizeof(value));return value;}
    std::uintptr_t address(const void* p){return reinterpret_cast<std::uintptr_t>(p);}
    MoneyFixture(){
        money_reset();game_test::money_files={};game_test::money_exists={};game_test::money_write_result=0;
        game_test::money_read_result=game_test::money_open_result=game_test::money_create_result=game_test::money_open_write_result=0;
        game_test::money_legacy_open_result=game_test::money_size_result=0;
        game_test::money_create_race=false;game_test::money_writes=0;game_test::money_write_flags=0;
        f.view_mode();put(f.image.data(),0xAFF39C,std::int32_t(2));put(f.input_state.data(),0x501C,std::int32_t(1234));
        put(save.data(),0x4D8,address(work.data()));save[0x1D]=3;put(work.data(),0xEAF8,std::uint32_t(0xC0FFEE));
        game_test::memory_maps={{main_base,f.image.size(),3},{address(f.actor),sizeof(f.actor),3},
            {address(f.input_state.data()),f.input_state.size(),3},{address(save.data()),save.size(),3},{address(work.data()),work.size(),3}};
        file_mounted=true;extended_upgrade_store_loaded=false;extended_upgrade_mask_known=true;
        extended_upgrade_loaded_identity={};extended_upgrade_mask=0;
    }
    std::int32_t wallet(){return get<std::int32_t>(f.input_state.data(),0x501C);}
    void load(unsigned slot=8,std::uint32_t crc=0x99887766){save[0x1D]=std::uint8_t(slot);put(work.data(),0xEAF8,crc);ctx={};re4::set_saved_x(ctx.bytes,19,address(save.data()));ExtendedLoad::Callback(&ctx);}
    void save_as(unsigned slot,std::uint32_t crc){save[0x1D]=std::uint8_t(slot);put(work.data(),0xEAF8,crc);ctx={};re4::set_saved_x(ctx.bytes,19,address(save.data()));ExtendedSave::Callback(&ctx);}
    void request(){control.money_request=control.money_request+1;PadNormalize::Callback(nullptr);}
    void legacy(unsigned index,bool claimed){
        auto& bytes=game_test::money_files[index];bytes.assign(33056,0);std::memcpy(bytes.data(),"R4MB",4);
        const std::uint32_t header[]={1,2,0,17};std::memcpy(bytes.data()+4,header,sizeof(header));bytes[33]=claimed?1:0;
        std::uint32_t sum=2166136261u;for(std::size_t i=0;i<bytes.size();++i){sum^=i>=20&&i<24?0:bytes[i];sum*=16777619u;}
        std::memcpy(bytes.data()+20,&sum,4);game_test::money_exists[index]=true;
    }
};
}
int main(){
    {
        MoneyFixture h;h.load();h.save_as(3,0xABCD);h.load(8,0x99887766);h.request();
        assert(h.wallet()==201234&&control.money_result==1&&!settings.extended_weapon_upgrade);
        assert(game_test::money_exists[2]&&game_test::money_files[2]==std::vector<std::uint8_t>{1}&&game_test::money_write_flags==1);
        assert(!game_test::money_exists[0]&&!game_test::money_exists[1]);
        h.request();assert(h.wallet()==201234&&control.money_result==2);
        h.save_as(9,0xBEEF);money_reset();h.load(9,0xBEEF);h.request();assert(h.wallet()==201234&&control.money_result==2);
        money_reset();h.load(5,0xBAD);h.request();assert(h.wallet()==201234&&control.money_result==2);
        ExtendedSnapshotRestore::Callback(h.work.data(),0,0,0);h.request();assert(h.wallet()==201234&&control.money_result==2);
    }
    for(auto contents:{std::vector<std::uint8_t>{},std::vector<std::uint8_t>{0},std::vector<std::uint8_t>{99,23}}){
        MoneyFixture h;game_test::money_exists[2]=true;game_test::money_files[2]=contents;h.request();
        assert(h.wallet()==1234&&control.money_result==2&&game_test::money_writes==0); // Presence alone is authoritative.
    }
    {
        MoneyFixture h;h.request();const auto writes=game_test::money_writes,opens=game_test::fs_opens;
        for(unsigned i=0;i<120;++i)PadNormalize::Callback(nullptr);
        assert(h.wallet()==201234&&game_test::money_writes==writes&&game_test::fs_opens==opens);
    }
    {
        MoneyFixture h;h.load();h.save_as(6,0xCAFE);assert(!game_test::money_exists[2]&&game_test::money_writes==0);
        h.request();assert(h.wallet()==201234);
    }
    for(unsigned index:{0u,1u}){
        MoneyFixture h;h.legacy(index,true);h.request();assert(h.wallet()==1234&&control.money_result==2&&game_test::money_exists[2]);
        money_reset();h.request();assert(h.wallet()==1234&&control.money_result==2);
    }
    {
        MoneyFixture h;h.legacy(0,false);h.legacy(1,false);h.request();assert(h.wallet()==201234);
    }
    for(unsigned fault:{0u,1u,2u}){
        MoneyFixture h;h.legacy(0,true);
        if(fault==0)game_test::money_legacy_open_result=9;
        if(fault==1)game_test::money_size_result=9;
        if(fault==2)game_test::money_read_result=9;
        h.request();
        assert(h.wallet()==1234&&control.money_result==4&&!game_test::money_exists[2]&&!money_legacy_checked);
        game_test::money_legacy_open_result=game_test::money_size_result=game_test::money_read_result=0;
        h.request();assert(h.wallet()==1234&&control.money_result==2&&game_test::money_exists[2]);
    }
    {
        MoneyFixture h;h.legacy(0,true);game_test::money_create_result=9;h.request();
        assert(h.wallet()==1234&&control.money_result==2&&!game_test::money_exists[2]);
        game_test::money_create_result=0;h.request();assert(h.wallet()==1234&&control.money_result==2);
        money_reset();h.request();assert(h.wallet()==1234&&control.money_result==2&&game_test::money_exists[2]);
    }
    {
        MoneyFixture h;h.legacy(0,true);game_test::money_files[0][400]^=1;h.request();assert(h.wallet()==201234); // Broken old histories no longer lock mid-game users out.
    }
    {
        MoneyFixture h;game_test::money_create_result=9;h.request();assert(h.wallet()==1234&&control.money_result==4&&!game_test::money_exists[2]);
        game_test::money_create_result=0;h.request();assert(h.wallet()==201234);
    }
    {
        MoneyFixture h;game_test::money_open_result=9;h.request();assert(h.wallet()==1234&&control.money_result==4&&!game_test::money_exists[2]);
        game_test::money_open_result=0;h.request();assert(h.wallet()==201234);
    }
    for(bool open_failure:{false,true}){
        MoneyFixture h;if(open_failure)game_test::money_open_write_result=9;else game_test::money_write_result=9;
        h.request();assert(h.wallet()==1234&&control.money_result==4&&game_test::money_exists[2]);
        game_test::money_open_write_result=game_test::money_write_result=0;money_reset();h.request();assert(h.wallet()==1234&&control.money_result==2);
    }
    {
        MoneyFixture h;game_test::money_create_race=true;h.request();assert(h.wallet()==1234&&control.money_result==2&&game_test::money_writes==0);
    }
    {
        MoneyFixture h;h.put(h.f.input_state.data(),0x501C,std::int32_t(99800000));h.request();assert(h.wallet()==99800000&&control.money_result==5&&!game_test::money_exists[2]);
        h.put(h.f.input_state.data(),0x501C,std::int32_t(99799999));h.request();assert(h.wallet()==99999999);
    }
    for(unsigned mode:{0u,0x1000u,0x4000u,0x800000u,0x40000000u,0x80000000u}){
        MoneyFixture h;if(!mode)h.put(h.f.image.data(),0xAFF39C,std::int32_t(0));else h.put(h.f.input_state.data(),0x78,std::uint32_t(mode));
        h.request();assert(h.wallet()==1234&&control.money_result==3&&game_test::money_writes==0);
    }
    {
        MoneyFixture h;control.money_request=1;h.load();PadNormalize::Callback(nullptr);assert(h.wallet()==1234&&control.money_ack==1);
        control.money_request=2;ExtendedSnapshotRestore::Callback(h.work.data(),0,0,0);PadNormalize::Callback(nullptr);assert(h.wallet()==1234&&control.money_ack==2);
    }
    for(int invalid:{-1,100000000}){
        MoneyFixture h;h.put(h.f.input_state.data(),0x501C,std::int32_t(invalid));h.request();assert(h.wallet()==invalid&&control.money_result==3);
    }
    {
        MoneyFixture h;game_test::memory_maps[2].perm=1;h.request();assert(h.wallet()==1234&&control.money_result==3);
        game_test::memory_maps[2].perm=3;h.f.actor[0x114]=1;h.request();assert(h.wallet()==1234&&control.money_result==3);
    }
    std::puts("actual money receipt: mid-game grant, presence, restarts, migration, amount and I/O PASS");
}
