#include "re4/update.hpp"
#include "nro_fixture.hpp"
#include "home_stubs/switch.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
Bytes read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
void write(const fs::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
int hash_file(const char* p,unsigned char* out){const auto bytes=read(p);if(bytes.empty())return 0;sha256CalculateHash(out,bytes.data(),bytes.size());return 1;}
struct Ops:re4::UpdateFileOps {
    int fail=0,calls=0;
    std::set<int> failures;
    int move(const char* from,const char* to)override{++calls;if(calls==fail || failures.count(calls))return -1;return std::rename(from,to);}
};
int main(){
    using namespace re4;ReleaseInfo info;
    const std::string good="RE4-CONTROLS-UPDATE 1\nversion=0.4.0\nbytes=8192\nsha256="+std::string(64,'a')+"\n";
    assert(parse_release_manifest(good,info) && info.version=="0.4.0" && info.bytes==8192);
    assert(newer_version("0.4.1","0.4.0") && newer_version("1.0.0","0.99.99"));
    assert(!newer_version("0.4.0","0.4.0") && !newer_version("0.3.0","0.4.0"));
    assert(!newer_version("0.4.1junk","0.4.0"));
    for(const auto& bad:{good+"url=https://evil.test\n",std::string("broken"),good.substr(0,good.size()-2),
            std::string("RE4-CONTROLS-UPDATE 1\nversion=../1\nbytes=8192\nsha256=")+std::string(64,'a')+"\n"})
        assert(!parse_release_manifest(bad,info));
    Bytes nro=own_nro_fixture("0.4.0");
    const fs::path root="build/update-test";fs::create_directories(root);
    const Bytes old_bridge{'b','r','i','d','g','e'},old_game{'g','a','m','e'};
    for(int fail=0;fail<=4;++fail){
        for(const auto& f:fs::directory_iterator(root))fs::remove(f.path());
        write(root/"bridge.nro",old_bridge);write(root/"game.nro",old_game);write(root/"update.nro.download",nro);
        info={};info.version="0.4.0";info.bytes=nro.size();hash_file((root/"update.nro.download").string().c_str(),info.sha256.data());
        Ops ops;ops.fail=fail;std::string detail;
        const bool result=publish_update(root.string(),info,hash_file,ops,detail);
        assert(result==(fail==0));
        assert(read(root/"bridge.nro")== (result?nro:old_bridge));
        assert(read(root/"game.nro")== (result?nro:old_game));
        assert(!detail.empty());
    }
    Ops mismatch;std::string mismatch_detail;auto wrong_version=info;wrong_version.version="0.4.1";
    assert(!publish_update(root.string(),wrong_version,hash_file,mismatch,mismatch_detail) && mismatch.calls==0);
    Ops ops;std::string detail;info.sha256[0]^=1;
    assert(!publish_update(root.string(),info,hash_file,ops,detail) && ops.calls==0);
    const fs::path first="build/self-install-test";fs::create_directories(first);
    for(const auto& file:fs::directory_iterator(first))fs::remove(file.path());
    Bytes self=own_nro_fixture("0.4.0");
    auto* nacp=self.data()+8192+40;
    write(first/"downloaded.nro",self);Ops bootstrap;
    assert(bootstrap_self_images((first/"downloaded.nro").string(),first.string(),hash_file,bootstrap,detail));
    assert(read(first/"bridge.nro")==self && read(first/"game.nro")==self);
    std::memcpy(nacp+0x3060,"0.3.0",5);write(first/"older.nro",self);
    const auto newest=read(first/"bridge.nro");
    assert(bootstrap_self_images((first/"older.nro").string(),first.string(),hash_file,bootstrap,detail));
    assert(read(first/"bridge.nro")==newest); // Opening an older NRO cannot downgrade installed slots.
    auto future=own_nro_fixture("0.5.0");write(first/"bridge.nro",future);fs::remove(first/"game.nro");
    assert(bootstrap_self_images((first/"older.nro").string(),first.string(),hash_file,bootstrap,detail));
    assert(read(first/"bridge.nro")==future && read(first/"game.nro")==future);
    for(const auto& f:fs::directory_iterator(root))fs::remove(f.path());
    write(root/"game.nro",old_game);write(root/"bridge.nro",old_bridge);write(root/"update.nro.download",nro);
    info={};info.version="0.4.0";info.bytes=nro.size();hash_file((root/"update.nro.download").string().c_str(),info.sha256.data());
    Ops interrupted;interrupted.failures={4,5};
    assert(!publish_update(root.string(),info,hash_file,interrupted,detail));
    assert(read(root/"game.nro.ota-backup")==old_game);
    Ops recover;assert(publish_update(root.string(),info,hash_file,recover,detail));
    assert(read(root/"game.nro")==nro && read(root/"bridge.nro")==nro);
    auto foreign=nro;foreign[8192+40+0x3038]^=1;write(root/"update.nro.download",foreign);
    hash_file((root/"update.nro.download").string().c_str(),info.sha256.data());Ops blocked;
    assert(!publish_update(root.string(),info,hash_file,blocked,detail) && blocked.calls==0);
    std::puts("OTA core: strict versions/manifest, real NRO+SHA verification and paired rollback PASS");
}
