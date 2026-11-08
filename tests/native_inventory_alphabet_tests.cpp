#include "re4/native_inventory_alphabet.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
    std::array<re4::NativeInventoryName,4> names{};
    const char16_t* words[]={u"Пистолет",u"Винтовка",u"Арбалет",u"Матильда"};
    for(unsigned i=0;i<4;++i)for(const auto* p=words[i];*p;++p)names[i].glyphs[names[i].count++]=unsigned(*p)-0x400+70;
    re4::NativeInventoryPrefix prefix;
    assert(re4::native_inventory_russian_prefix(names,prefix));
    const unsigned expected[]={86,136,118,128,118,13};
    assert(prefix.count==6);for(unsigned i=0;i<6;++i)assert(prefix.glyphs[i]==expected[i]);
    const auto good=prefix;
    names[1].glyphs[3]++;assert(!re4::native_inventory_russian_prefix(names,prefix));
    assert(!std::memcmp(&good,&prefix,sizeof(prefix)));
    names[1].glyphs[3]--;names[2].count--;assert(!re4::native_inventory_russian_prefix(names,prefix));
    names[2].count++;names[2].glyphs[0]=names[2].glyphs[3];assert(!re4::native_inventory_russian_prefix(names,prefix));
    // Exact native wrappers only, with one optional trailing space before controls.
    const std::uint16_t encoded[]={0,0xA7,0xC1,0x80,14,8,1};
    re4::NativeInventoryName name;
    assert(re4::native_inventory_name(encoded,7,name)&&name.count==2&&name.glyphs[0]==39&&name.glyphs[1]==65);
    auto malformed=std::array<std::uint16_t,7>{0,0xA7,0xC1,0x80,15,8,1};
    assert(!re4::native_inventory_name(malformed.data(),7,name));
    malformed[4]=14;malformed[5]=0;assert(!re4::native_inventory_name(malformed.data(),7,name));
    assert(!re4::native_inventory_name(encoded,6,name));
    std::puts("native inventory alphabet: localized cross-word constraints and strict MDT wrappers PASS");
}
