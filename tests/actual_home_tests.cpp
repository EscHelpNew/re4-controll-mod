#include "../source/diagnostic/home_launcher.cpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
int main(){
    using namespace re4;
    std::ifstream input("assets/home-launcher.bin",std::ios::binary);
    std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(input)),{});
    HomeBundle parsed;assert(parse_home_bundle(raw.data(),raw.size(),re4::verify,parsed));
    for(unsigned offset:{0u,8u,20u,24u,128u,176u,180u,190u,320u}){
        auto broken=raw;broken[offset]^=1;HomeBundle unchanged;unchanged.title=42;
        assert(!parse_home_bundle(broken.data(),broken.size(),re4::verify,unchanged));
        assert(unchanged.title==42);
    }
    assert(!parse_home_bundle(raw.data(),raw.size()-1,re4::verify,parsed));
    for(auto location:{NcmStorageId_SdCard,NcmStorageId_BuiltInUser}){
        for(const char* failure:{"","set","commit","record"}){
            home_test::reset();home_test::installed=true;home_test::location=location;
            auto old_metadata=parsed.database;old_metadata[72]^=1;
            home_test::metadata=old_metadata;
            const std::string old_id(reinterpret_cast<const char*>(old_metadata.data()+72),16);
            home_test::contents[old_id]={1,2,3};home_test::failure=failure;
            std::string detail;
            const auto result=install_own_home_launcher("assets/home-launcher.bin",detail);
            assert(result==(*failure?HomeResult::Failed:HomeResult::Installed));
            assert(home_test::metadata==(*failure?old_metadata:parsed.database));
            assert((home_test::contents.at(old_id)==std::vector<std::uint8_t>{1,2,3}));
            assert(home_test::last_location==location);
            assert(home_test::ns==0 && home_test::ncm==0 && home_test::stores==0 && home_test::databases==0);
        }
    }
    for(const char* failure:{"generate","create","write","flush","register","set","commit","record"}){
        home_test::reset();home_test::failure=failure;std::string detail;
        assert(install_own_home_launcher("assets/home-launcher.bin",detail)==HomeResult::Failed);
        assert(!detail.empty() && home_test::metadata.empty() && home_test::placeholder.empty());
        assert(home_test::ns==0 && home_test::ncm==0 && home_test::stores==0 && home_test::databases==0);
    }
    home_test::reset();std::string detail;
    assert(install_own_home_launcher("assets/home-launcher.bin",detail)==HomeResult::Installed);
    assert(home_test::contents.size()==3 && home_test::records==1 && home_test::refreshes==1);
    const auto before=home_test::contents;
    assert(install_own_home_launcher("assets/home-launcher.bin",detail)==HomeResult::Current);
    assert(home_test::contents==before && home_test::refreshes==2);
    home_test::failure="refresh";
    assert(install_own_home_launcher("assets/home-launcher.bin",detail)==HomeResult::NeedsRefresh);
    assert(!home_test::metadata.empty());
    home_test::applet=AppletType_LibraryApplet;
    assert(install_own_home_launcher("assets/home-launcher.bin",detail)==HomeResult::Failed);
    assert(home_test::ns==0 && home_test::ncm==0);
    std::puts("actual HOME installer: real bundle/SHA256, SDK failures, rollback, idempotence and cleanup PASS");
}
