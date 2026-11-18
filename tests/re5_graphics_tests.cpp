#include "re5/graphics.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 graphics line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
int main(){
 using re4::TextureFilter;std::array<std::uint8_t,56> original{},out{};
 for(unsigned i=0;i<original.size();++i)original[i]=std::uint8_t(i*3+1);
 const std::uint32_t keep=0xa8000000u|(2u<<8)|(3u<<11)|(4u<<14)|(5u<<22);
 const auto flags=[&](unsigned mode){return keep|(9u<<17)|mode;};
 for(unsigned native=0;native<18;++native){
  auto word=flags(native);std::memcpy(original.data(),&word,4);
  assert(!re5::filter_sampler_descriptor(original.data(),out.data(),out.size(),TextureFilter::Native)&&out==original);
  for(auto setting:{TextureFilter::Bilinear,TextureFilter::Trilinear,TextureFilter::Aniso2,TextureFilter::Aniso4,TextureFilter::Aniso8,TextureFilter::Aniso16}){
   const bool changed=re5::filter_sampler_descriptor(original.data(),out.data(),out.size(),setting);
   assert(changed==((native==7||native==8)&&!(native==8&&setting==TextureFilter::Aniso8)));
   if(native!=7&&native!=8){assert(out==original);continue;}
   std::uint32_t result=0;std::memcpy(&result,out.data(),4);
   const unsigned mode=setting==TextureFilter::Bilinear?6:setting==TextureFilter::Trilinear?7:8;
   const unsigned index=setting==TextureFilter::Aniso2?2:setting==TextureFilter::Aniso4?4:setting==TextureFilter::Aniso8?9:setting==TextureFilter::Aniso16?16:0;
   assert(result==(keep|(index<<17)|mode));
   assert(std::memcmp(out.data()+4,original.data()+4,52)==0);
  }
 }
 auto word=flags(7);std::memcpy(original.data(),&word,4);
 auto inplace=original;assert(re5::filter_sampler_descriptor(inplace.data(),inplace.data(),56,TextureFilter::Aniso16));
 std::array<std::uint8_t,60> overlap{};std::memcpy(overlap.data(),original.data(),56);
 assert(re5::filter_sampler_descriptor(overlap.data(),overlap.data()+4,56,TextureFilter::Aniso16));
 assert(std::memcmp(overlap.data()+4,inplace.data(),56)==0);
 out.fill(77);const auto untouched=out;
 assert(!re5::filter_sampler_descriptor(original.data(),out.data(),55,TextureFilter::Aniso16)&&out==untouched);
 assert(!re5::filter_sampler_descriptor(nullptr,out.data(),56,TextureFilter::Aniso16)&&out==untouched);
 assert(!re5::filter_sampler_descriptor(original.data(),nullptr,56,TextureFilter::Aniso16));
 std::puts("RE5 sampler: native modes preserved; bilinear/trilinear/AF2-16 alter only proven fields PASS");
}
