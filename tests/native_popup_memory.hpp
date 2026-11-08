#pragma once
// Test-only live MDT/font data for fixtures that exercise popup submission.
// Include after game_module_tests.cpp; production reads the same adapter fields.
struct NativePopupMemory {
    std::vector<std::uint8_t> core=std::vector<std::uint8_t>(0x4000);
    std::array<std::array<std::int8_t,2>,512> metrics{};
    template<class T>static void put(void* p,std::size_t at,T value){std::memcpy(static_cast<std::uint8_t*>(p)+at,&value,sizeof(value));}
    void bind(Fixture& f){
        const auto font=f.image.data()+0x124FAB0,manager=f.image.data()+0x124FFB0,bank=core.data()+0x100;
        put(f.input_state.data(),0x60,core.data());put(core.data(),0x54,std::uint32_t(0x100));put(core.data(),0x58,std::uint32_t(0x3000));
        put(manager,0,std::int32_t(1));put(manager,0x20,bank);put(manager,0x44,std::uint32_t(1));manager[0x53]=1;
        put(bank,4,std::uint32_t(0x110));
        for(unsigned raw=0;raw<0x110;++raw)put(bank,8+raw*8,std::uint64_t(0x1000));
        constexpr std::array<std::uint16_t,6> name{0,0xA7,0x1AC,14,8,1};
        std::memcpy(bank+0x1000,name.data(),sizeof(name));
        for(auto& metric:metrics)metric={0,32};
        font[0]=1;font[0xFC]=font[0xFD]=64;put(font,0xF8,std::uint16_t(2048));put(font,0xFA,std::uint16_t(1024));put(font,0xF0,metrics.data());
        put(f.image.data(),0xB00614,std::uint32_t(0xE5D9CFF0));
        game_test::memory_maps.push_back({main_base,f.image.size(),3});
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3});
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(core.data()),core.size(),3});
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(metrics.data()),sizeof(metrics),3});
        inventory_damage_glyph_native.draw=+[](const re4::NativeInventoryGlyph*){};
        native_gx_blend=+[](unsigned a,unsigned b,unsigned c,unsigned d){assert(a==1&&!d&&((b==4&&(c==1||c==5))||(b==1&&!c)));};
    }
};
