#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
inline std::vector<unsigned char> own_nro_fixture(const char* version){
    std::vector<unsigned char> bytes(8192+40+0x4000);
    auto put=[&](unsigned offset,unsigned value){std::memcpy(bytes.data()+offset,&value,4);};
    std::memcpy(bytes.data()+0x10,"NRO0",4);
    put(0x18,8192);put(0x24,4096);put(0x28,4096);put(0x2c,4096);put(0x30,8192);
    std::memcpy(bytes.data()+4096,"GAME_REMOTE_RELOAD_BRIDGE_V2",28);
    std::memcpy(bytes.data()+8192,"ASET",4);
    const std::uint64_t offset=40,size=0x4000,title=0x0100E4C04E540000ull;
    std::memcpy(bytes.data()+8192+24,&offset,8);std::memcpy(bytes.data()+8192+32,&size,8);
    auto* nacp=bytes.data()+8192+40;
    std::memcpy(nacp,"RE4 Controls",12);std::memcpy(nacp+0x200,"re4-controls",12);
    std::memcpy(nacp+0x3038,&title,8);std::memcpy(nacp+0x3060,version,std::strlen(version));
    return bytes;
}
