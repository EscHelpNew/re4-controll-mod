#pragma once
#include <cstring>
#include <type_traits>
#include <cstddef>
#include <cstdint>
#include <limits>
namespace re5 {
struct MemoryReader {
 void* context=nullptr;
 bool (*read)(void*,std::uintptr_t,void*,std::size_t)=nullptr;
 bool (*readable)(void*,std::uintptr_t,std::size_t)=nullptr;
 bool range(std::uintptr_t base,std::size_t size)const{
  return base&&size&&base<=std::numeric_limits<std::uintptr_t>::max()-size&&readable&&readable(context,base,size);
 }
 template<class T>bool get(std::uintptr_t base,std::size_t offset,T& out)const{
  if(!base||offset>std::numeric_limits<std::uintptr_t>::max()-base)return false;
  const auto address=base+offset;
  if(!read||address>std::numeric_limits<std::uintptr_t>::max()-sizeof(T))return false;
  static_assert(std::is_trivially_copyable<T>::value,"native snapshot must be trivial");
  T value{};if(!read(context,address,&value,sizeof(value)))return false;std::memcpy(&out,&value,sizeof(value));return true;
 }
};
}
