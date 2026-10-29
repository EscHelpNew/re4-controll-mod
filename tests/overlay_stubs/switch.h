#pragma once
#include <cstdint>
#include <cstddef>
using u64=std::uint64_t;using u32=std::uint32_t;using u16=std::uint16_t;using u8=std::uint8_t;
using Result=unsigned;
struct Service {bool active=false;};
struct MemoryInfo {u64 addr,size;u32 type,perm;};
constexpr unsigned SfBufferAttr_HipcMapAlias=1,SfBufferAttr_Out=2,SfBufferAttr_In=4;
constexpr unsigned MemType_CodeMutable=4,Perm_Rw=3;
#define R_SUCCEEDED(v) ((v)==0)
#define R_FAILED(v) ((v)!=0)
struct TestDispatchParams {
    unsigned buffer_attrs[1]{};
    struct Buffer {const void* data=nullptr;std::size_t size=0;} buffers[1]{};
};
Result test_dispatch(Service*,unsigned,const void*,std::size_t,void*,std::size_t,TestDispatchParams);
#define serviceDispatch(s,c,...) test_dispatch(s,c,nullptr,0,nullptr,0,TestDispatchParams{__VA_ARGS__})
#define serviceDispatchOut(s,c,o,...) test_dispatch(s,c,nullptr,0,&o,sizeof(o),TestDispatchParams{__VA_ARGS__})
#define serviceDispatchIn(s,c,i,...) test_dispatch(s,c,&i,sizeof(i),nullptr,0,TestDispatchParams{__VA_ARGS__})
#define serviceDispatchInOut(s,c,i,o,...) test_dispatch(s,c,&i,sizeof(i),&o,sizeof(o),TestDispatchParams{__VA_ARGS__})
Result pminfoInitialize();
void pminfoExit();
Result smGetService(Service*,const char*);
bool serviceIsActive(Service*);
void serviceClose(Service*);
Result pmdmntGetApplicationProcessId(u64*);
Result pminfoGetProgramId(u64*,u64);
constexpr u64 HidNpadButton_A=1, HidNpadButton_Left=2, HidNpadButton_Right=4;
