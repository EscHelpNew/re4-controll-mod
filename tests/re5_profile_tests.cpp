#include "re5/profile.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 profile line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
template<class Sites>void seed(std::vector<std::uint8_t>& bytes,std::size_t offset,const re5::profile::BuildId& id,const Sites& sites){
 std::memcpy(bytes.data()+offset,id.data(),id.size());for(auto site:sites)std::memcpy(bytes.data()+site.offset,&site.instruction,4);
}
int main(){
 using namespace re5::profile;
 std::vector<std::uint8_t> main(main_size),sdk(sdk_size);
 seed(main,build_id_offset,build_id,main_sites);seed(sdk,sdk_build_id_offset,sdk_build_id,sdk_sites);
 assert(matches(main.data(),main.size())&&sdk_matches(sdk.data(),sdk.size()));
 assert(!matches(nullptr,main.size())&&!matches(main.data(),main_size-1));
 assert(!sdk_matches(nullptr,sdk.size())&&!sdk_matches(sdk.data(),sdk_size-1));
 for(unsigned i=0;i<32;++i){main[build_id_offset+i]^=1;assert(!matches(main.data(),main.size()));main[build_id_offset+i]^=1;}
 for(auto site:main_sites){main[site.offset]^=1;assert(!matches(main.data(),main.size()));main[site.offset]^=1;}
 for(auto site:sdk_sites){sdk[site.offset]^=1;assert(!sdk_matches(sdk.data(),sdk.size()));sdk[site.offset]^=1;}
 std::vector<std::uint8_t> old(main_size);
 seed(old,legacy_build_id_offset,legacy_build_id,legacy_main_sites);
 assert(detect(old.data(),old.size())==Build::Legacy&&detect(main.data(),main.size())==Build::Latest);
 for(unsigned i=0;i<32;++i){old[legacy_build_id_offset+i]^=1;assert(!matches(old.data(),old.size()));old[legacy_build_id_offset+i]^=1;}
 for(auto site:legacy_main_sites){old[site.offset]^=1;assert(!matches(old.data(),old.size()));old[site.offset]^=1;}
 assert(!matches(old.data(),main_size-1));
 selected_build=Build::Legacy;
 assert(offset<0x7CA660>()==0x7CA2E0&&offset<0x24AAE8>()==0x24A908&&offset<0x4D2FD0>()==0x4D2DF0);
 assert(offset<0x1CF8CC8>()==0x1CF8CC8&&legacy_offset(0x1234)==0);
 selected_build=Build::Latest;assert(offset<0x7CA660>()==0x7CA660);
 assert(!matches(sdk.data(),sdk.size()));
 std::puts("RE5 native identity: full Build IDs, module bounds and every protected instruction PASS");
}
