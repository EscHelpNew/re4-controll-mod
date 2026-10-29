#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <vector>
using u8=std::uint8_t;using u32=std::uint32_t;using u64=std::uint64_t;using s32=std::int32_t;using s64=std::int64_t;using Result=u32;
#define R_SUCCEEDED(rc) ((rc)==0)
#define R_FAILED(rc) ((rc)!=0)
enum {AppletType_Application,AppletType_SystemApplication,AppletType_LibraryApplet};
enum NcmStorageId {NcmStorageId_BuiltInUser=4,NcmStorageId_SdCard=5};
enum {NcmContentMetaType_Application=0x80,NcmContentInstallType_Full=0};
struct NcmContentId {u8 c[16];};struct NcmPlaceHolderId {unsigned id;};
struct NcmContentStorage {};struct NcmContentMetaDatabase {};
struct NcmContentMetaKey {u64 id;u32 version;u8 type,install_type,padding[2];};
struct NsApplicationContentMetaStatus {u8 meta_type,storageID,rights_check,reserved;u32 version;u64 application_id;};
struct Service {bool active=false;};
namespace home_test {
using Bytes=std::vector<u8>;
inline constexpr u64 own=0x0100E4C04E540000ull;
inline int applet=AppletType_Application,ns=0,ncm=0,stores=0,databases=0,refreshes=0,records=0;
inline bool installed=false;
inline NcmStorageId location=NcmStorageId_SdCard,last_location=NcmStorageId_SdCard;
inline std::string failure;
inline std::map<std::string,Bytes> contents;
inline Bytes metadata,placeholder;
inline Result result(const char* name){if(failure==name){failure.clear();return 5;}return 0;}
inline std::string name(const NcmContentId* id){return {reinterpret_cast<const char*>(id->c),16};}
inline void reset(){applet=AppletType_Application;ns=ncm=stores=databases=refreshes=records=0;installed=false;location=last_location=NcmStorageId_SdCard;failure.clear();contents.clear();metadata.clear();placeholder.clear();}
}
inline int appletGetAppletType(){return home_test::applet;}
inline Result nsInitialize(){++home_test::ns;return 0;}inline void nsExit(){--home_test::ns;}
inline Result ncmInitialize(){++home_test::ncm;return 0;}inline void ncmExit(){--home_test::ncm;}
inline bool serviceIsActive(Service* s){return s->active;}inline void serviceClose(Service* s){s->active=false;}
inline Result nsGetApplicationManagerInterface(Service* s){s->active=true;return 0;}
inline Result nsIsAnyApplicationEntityInstalled(u64 id,bool* b){assert(id==home_test::own);*b=home_test::installed;return 0;}
inline Result nsListApplicationContentMetaStatus(u64 id,s32,NsApplicationContentMetaStatus* out,s32,s32* count){assert(id==home_test::own);*count=1;out[0]={0x80,u8(home_test::location),0,0,0,id};return 0;}
inline Result ncmOpenContentStorage(NcmContentStorage*,NcmStorageId location){++home_test::stores;home_test::last_location=location;return 0;}
inline Result ncmOpenContentMetaDatabase(NcmContentMetaDatabase*,NcmStorageId){++home_test::databases;return 0;}
inline void ncmContentStorageClose(NcmContentStorage*){--home_test::stores;}
inline void ncmContentMetaDatabaseClose(NcmContentMetaDatabase*){--home_test::databases;}
inline void check_key(const NcmContentMetaKey* k){assert(k->id==home_test::own && k->type==0x80 && k->version==0);}
inline Result ncmContentMetaDatabaseHas(NcmContentMetaDatabase*,bool* exists,const NcmContentMetaKey* k){check_key(k);*exists=!home_test::metadata.empty();return 0;}
inline Result ncmContentMetaDatabaseGetSize(NcmContentMetaDatabase*,u64* size,const NcmContentMetaKey* k){check_key(k);*size=home_test::metadata.size();return 0;}
inline Result ncmContentMetaDatabaseGet(NcmContentMetaDatabase*,const NcmContentMetaKey* k,u64* size,void* data,u64 capacity){check_key(k);assert(capacity>=home_test::metadata.size());*size=home_test::metadata.size();std::memcpy(data,home_test::metadata.data(),*size);return 0;}
inline Result ncmContentMetaDatabaseSet(NcmContentMetaDatabase*,const NcmContentMetaKey* k,const void* data,u64 size){check_key(k);if(auto rc=home_test::result("set"))return rc;const auto* bytes=static_cast<const u8*>(data);home_test::metadata.assign(bytes,bytes+size);return 0;}
inline Result ncmContentMetaDatabaseRemove(NcmContentMetaDatabase*,const NcmContentMetaKey* k){check_key(k);home_test::metadata.clear();return 0;}
inline Result ncmContentMetaDatabaseCommit(NcmContentMetaDatabase*){return home_test::result("commit");}
inline Result ncmContentStorageHas(NcmContentStorage*,bool* has,const NcmContentId* id){*has=home_test::contents.count(home_test::name(id));return 0;}
inline Result ncmContentStorageGetSizeFromContentId(NcmContentStorage*,s64* size,const NcmContentId* id){*size=home_test::contents.at(home_test::name(id)).size();return 0;}
inline Result ncmContentStorageReadContentIdFile(NcmContentStorage*,void* data,std::size_t size,const NcmContentId* id,s64 offset){assert(offset==0);const auto& bytes=home_test::contents.at(home_test::name(id));assert(size==bytes.size());std::memcpy(data,bytes.data(),size);return 0;}
inline Result ncmContentStorageGeneratePlaceHolderId(NcmContentStorage*,NcmPlaceHolderId* id){id->id=1;return home_test::result("generate");}
inline Result ncmContentStorageCreatePlaceHolder(NcmContentStorage*,const NcmContentId*,const NcmPlaceHolderId*,s64 size){if(auto rc=home_test::result("create"))return rc;home_test::placeholder.resize(size);return 0;}
inline Result ncmContentStorageWritePlaceHolder(NcmContentStorage*,const NcmPlaceHolderId*,u64 offset,const void* data,std::size_t size){assert(offset==0 && size==home_test::placeholder.size());if(auto rc=home_test::result("write"))return rc;std::memcpy(home_test::placeholder.data(),data,size);return 0;}
inline Result ncmContentStorageFlushPlaceHolder(NcmContentStorage*){return home_test::result("flush");}
inline Result ncmContentStorageRegister(NcmContentStorage*,const NcmContentId* id,const NcmPlaceHolderId*){if(auto rc=home_test::result("register"))return rc;home_test::contents[home_test::name(id)]=home_test::placeholder;home_test::placeholder.clear();return 0;}
inline Result ncmContentStorageDeletePlaceHolder(NcmContentStorage*,const NcmPlaceHolderId*){home_test::placeholder.clear();return 0;}
struct SfArgs {unsigned buffer_attrs[1]{};struct {const void* ptr=nullptr;std::size_t size=0;} buffers[1];};
enum {SfBufferAttr_HipcMapAlias=1,SfBufferAttr_In=2};
template<class T> Result home_dispatch(unsigned command,const T& in,SfArgs args){
    if constexpr(std::is_integral_v<T>){assert(command==404 && in==home_test::own);++home_test::refreshes;return home_test::result("refresh");}
    else {assert(command==16 && in.title==home_test::own && args.buffers[0].size==24);++home_test::records;if(auto rc=home_test::result("record"))return rc;home_test::installed=true;return 0;}
}
#define serviceDispatchIn(s,cmd,in,...) home_dispatch(cmd,in,SfArgs{__VA_ARGS__})
inline void sha256CalculateHash(void* out,const void* data,std::size_t size){
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;DWORD bytes=32;
    assert(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT));
    assert(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash));
    assert(CryptHashData(hash,static_cast<const BYTE*>(data),DWORD(size),0));
    assert(CryptGetHashParam(hash,HP_HASHVAL,static_cast<BYTE*>(out),&bytes,0));
    CryptDestroyHash(hash);CryptReleaseContext(provider,0);
}
