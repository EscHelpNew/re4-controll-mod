#include "re4/install.hpp"
#include "re4/game_target.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <cstdint>
#include <array>
#include <vector>
#include <cerrno>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#undef assert
#define assert(c) do {if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
namespace fs = std::filesystem;
void put(const fs::path& p, const std::string& bytes) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary).write(bytes.data(), bytes.size());
}


// Compile the actual installer with test-only filesystem interposition, matching
// the native transport fixture. All uninjected operations use the real disk.
namespace installer_fault_fs {
using namespace std;
enum class Failure {None, FirstRecoveryWrite, AllRecoveryWrites, RecoveryReadback};
Failure failure=Failure::None;
std::string receipt;
bool receipt_removed=false;
unsigned recovery_writes=0,failed_reads=0;
int rename(const char* from,const char* to) {
    if(failure!=Failure::None&&fs::path(to)==fs::path(receipt)
       &&fs::path(from)==fs::path(receipt+".incoming")) {
        assert(!fs::exists(to)); // Fault occurs after old ownership proof removal.
        receipt_removed=true;
        errno=EIO;
        return -1;
    }
    return std::rename(from,to);
}
FILE* fopen(const char* path,const char* mode) {
    if(receipt_removed&&fs::path(path)==fs::path(receipt)) {
        if(!std::strcmp(mode,"wb")) {
            ++recovery_writes;
            if(failure==Failure::AllRecoveryWrites||recovery_writes==1) {
                errno=EIO;
                return nullptr;
            }
        }
        if(!std::strcmp(mode,"rb")&&failure==Failure::RecoveryReadback) {
            ++failed_reads;
            errno=EIO;
            return nullptr;
        }
    }
    return std::fopen(path,mode);
}
}
namespace installer_fault {
using re4::GameTarget;
using re4::KnownInstallFile;
using re4::InstallCatalog;
}
#define std installer_fault_fs
#define re4 installer_fault
#include "../source/install.cpp"
#undef re4
#undef std

std::string get(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
std::string old_re6_receipt(const char* tag, const std::string& module, const std::string& npdm,
                            const std::string& telemetry, const std::string& controls) {
    std::string receipt(tag, 8);
    for (const auto* bytes : {&module, &npdm, &telemetry, &controls}) {
        if (receipt.size() == 32 && !std::strcmp(tag, "RE6INST1")) break;
        std::uint64_t value=14695981039346656037ull;
        for (unsigned char byte : *bytes) value=(value^byte)*1099511628211ull;
        receipt.append(reinterpret_cast<const char*>(&value),8);
    }
    return receipt;
}



void receipt_recovery_fault_tests(const fs::path& root,const std::string& nso,
                                 const std::string& npdm,const std::string& ovl) {
    const auto payload=root/"recovery-payload";
    put(payload/"re6-subsdk9",nso);put(payload/"re6-main.npdm",npdm);put(payload/"re6-controls.ovl",ovl);
    auto old_module=nso;old_module[32]='L';auto old_npdm=npdm;old_npdm[32]='L';
    auto telemetry=ovl;telemetry[32]='T';auto controls=ovl;controls[32]='C';
    for(auto failure:{installer_fault_fs::Failure::AllRecoveryWrites,
                      installer_fault_fs::Failure::FirstRecoveryWrite,
                      installer_fault_fs::Failure::RecoveryReadback}) {
        const auto sd=root/("receipt-recovery-"+std::to_string(unsigned(failure)));
        const auto module=sd/"atmosphere/contents/01002A000CD48000/exefs/subsdk9";
        const auto meta=module.parent_path()/"main.npdm";
        const auto tel=sd/"switch/.overlays/re6-telemetry.ovl",ctl=sd/"switch/.overlays/re6-controls.ovl";
        const auto rec=sd/"switch/re4-controls/installed-re6-files.bin";
        const auto legacy=old_re6_receipt("RE6INST2",old_module,old_npdm,telemetry,controls);
        put(module,old_module);put(meta,old_npdm);put(tel,telemetry);put(ctl,controls);put(rec,legacy);
        put(sd/"switch/re4-controls/re6-controls.cfg","keep config");
        put(sd/"switch/re4-controls/captures/saved.csv","keep captured data");
        installer_fault_fs::failure=failure;
        installer_fault_fs::receipt=rec.string();
        installer_fault_fs::receipt_removed=false;
        installer_fault_fs::recovery_writes=installer_fault_fs::failed_reads=0;
        const auto* result=installer_fault::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6,{});
        installer_fault_fs::failure=installer_fault_fs::Failure::None;
        installer_fault_fs::receipt_removed=false;
        if(failure==installer_fault_fs::Failure::FirstRecoveryWrite) {
            assert(std::strstr(result,"rolled back"));
            assert(get(rec)==legacy);
        } else {
            assert(std::strstr(result,"rollback incomplete"));
            if(failure==installer_fault_fs::Failure::AllRecoveryWrites)assert(!fs::exists(rec));
            else {assert(get(rec)==legacy);assert(installer_fault_fs::failed_reads>0);}
        }
        assert(installer_fault_fs::recovery_writes>=2);
        assert(get(rec.string()+".re4-controls.previous")==legacy);
        assert(get(module)==old_module&&get(meta)==old_npdm&&get(tel)==telemetry&&get(ctl)==controls);
        assert(get(sd/"switch/re4-controls/re6-controls.cfg")=="keep config");
        assert(get(sd/"switch/re4-controls/captures/saved.csv")=="keep captured data");
    }
}

void telemetry_startup_tests(const fs::path& root, const std::string& nso,
                             const std::string& npdm, const std::string& ovl) {
    const auto sd=root/"retire-startup", overlays=sd/"switch/.overlays";
    const auto telemetry=overlays/"re6-telemetry.ovl",controls=overlays/"re6-controls.ovl";
    const auto receipt=sd/"switch/re4-controls/installed-re6-files.bin";
    const auto module=sd/"atmosphere/contents/01002A000CD48000/exefs/subsdk9";
    auto captured=ovl;captured[40]='T';
    auto changed=captured;changed[41]='X';
    assert(std::strstr(re4::retire_telemetry_overlay(nullptr),"Conflict"));
    assert(std::strstr(re4::retire_telemetry_overlay(sd.string().c_str()),"absent"));
    // Missing, corrupt and new-format receipts cannot prove ownership of telemetry.
    for(const auto& bad:{std::string(),std::string("invalid"),old_re6_receipt("RE6INST3",nso,npdm,ovl,ovl).substr(0,32)}) {
        put(telemetry,captured);put(receipt,bad);
        assert(std::strstr(re4::retire_telemetry_overlay(sd.string().c_str()),"Conflict"));
        assert(get(telemetry)==captured&&!fs::exists(telemetry.string()+".re4-controls.disabled"));
    }
    for(const auto* tag:{"RE6INST1","RE6INST2"}) {
        auto unowned=old_re6_receipt(tag,nso,npdm,captured,ovl);
        std::memset(unowned.data()+24,0,8);put(receipt,unowned);put(telemetry,captured);
        assert(std::strstr(re4::retire_telemetry_overlay(sd.string().c_str()),"ownership"));
        assert(get(telemetry)==captured&&get(receipt)==unowned);
        const auto legacy=old_re6_receipt(tag,nso,npdm,captured,ovl);
        put(receipt,legacy);put(telemetry,changed);
        assert(std::strstr(re4::retire_telemetry_overlay(sd.string().c_str()),"Conflict"));
        assert(get(telemetry)==changed&&get(receipt)==legacy);
        put(telemetry,captured);put(module,nso);put(controls,ovl);
        put(sd/"switch/re4-controls/telemetry.cfg","capture config");
        put(sd/"switch/re4-controls/captures/recording.csv","capture data");
        assert(std::strstr(re4::retire_telemetry_overlay(sd.string().c_str()),"Disabled"));
        assert(!fs::exists(telemetry)&&get(telemetry.string()+".re4-controls.disabled")==captured);
        assert(get(module)==nso&&get(controls)==ovl&&get(receipt)==legacy);
        assert(get(sd/"switch/re4-controls/telemetry.cfg")=="capture config");
        assert(get(sd/"switch/re4-controls/captures/recording.csv")=="capture data");
        assert(std::strstr(re4::retire_telemetry_overlay(sd.string().c_str()),"absent"));
    }
    // Preserve a different earlier backup, using an identity suffix for our file.
    const auto base=telemetry.string()+".re4-controls.disabled";
    put(base,"earlier telemetry backup");put(telemetry,captured);
    assert(std::strstr(re4::retire_telemetry_overlay(sd.string().c_str()),"Disabled"));
    assert(get(base)=="earlier telemetry backup");
    fs::path suffixed;
    for(const auto& entry:fs::directory_iterator(overlays)) {
        const auto name=entry.path().filename().string();
        if(name.find("re6-telemetry.ovl.re4-controls.disabled-")==0)suffixed=entry.path();
    }
    assert(!suffixed.empty()&&get(suffixed)==captured);
    put(suffixed,"another unrelated backup");put(telemetry,captured);
    assert(std::strstr(re4::retire_telemetry_overlay(sd.string().c_str()),"Conflict"));
    assert(get(telemetry)==captured&&get(suffixed)=="another unrelated backup");
    // Both old formats disable exactly their recorded members. INST1 has no
    // controls ownership; INST3 has no telemetry ownership.
    fs::remove(base);fs::remove(suffixed);
    for(const auto* tag:{"RE6INST1","RE6INST2","RE6INST3"}) {
        put(module,nso);put(module.parent_path()/"main.npdm",npdm);
        put(telemetry,captured);put(controls,ovl);
        const auto receipt_bytes=!std::strcmp(tag,"RE6INST3")
            ? old_re6_receipt("RE6INST1",nso,npdm,ovl,ovl)
            : old_re6_receipt(tag,nso,npdm,captured,ovl);
        auto owned=receipt_bytes;std::memcpy(owned.data(),tag,8);put(receipt,owned);
        assert(std::strstr(re4::disable_game_files(sd.string().c_str(),re4::GameTarget::RE6),"Disabled"));
        assert(!fs::exists(module)&&!fs::exists(module.parent_path()/"main.npdm"));
        assert(fs::exists(telemetry)==!std::strcmp(tag,"RE6INST3"));
        assert(fs::exists(controls)==!std::strcmp(tag,"RE6INST1"));
    }
}

int main() {
    const fs::path root = "build/install-test";
    fs::remove_all(root);
    const auto payload = root / "payload", sd = root / "sd";
    fs::create_directories(sd);
    assert(std::strstr(re4::install_game_files(payload.string().c_str(), sd.string().c_str()), "missing"));
    std::string nso(256, '\0'), npdm(256, '\0'), ovl(256, '\0');
    nso.replace(0, 4, "NSO0"); npdm.replace(0, 4, "META"); ovl.replace(16, 4, "NRO0");
    receipt_recovery_fault_tests(root,nso,npdm,ovl);
    telemetry_startup_tests(root,nso,npdm,ovl);
    put(payload / "subsdk9", nso); put(payload / "main.npdm", npdm); put(payload / "re4-controls.ovl", ovl);
    {
        // Manual own-module installation followed by overlay-only installation
        // records zero module ownership. Recognize only a trusted exact old file.
        const auto manual=root/"manual-sd";
        const auto old_module=manual/"atmosphere/contents/010099A00BC1E000/exefs/subsdk9";
        auto legacy=nso;legacy[32]='L';put(old_module,legacy);
        put(manual/"atmosphere/contents/010099A00BC1E000/exefs/main.npdm",npdm);
        assert(std::strstr(re4::install_overlay_only(payload.string().c_str(),manual.string().c_str()),"Installed"));
        const re4::KnownInstallFile approved[]{ {re4::GameTarget::RE4,0,256,0xE92A3B302C25C51Dull} };
        const re4::InstallCatalog catalog{approved,1};
        auto changed=legacy;changed[33]='X';put(old_module,changed);
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,catalog),"Conflict"));
        assert(!fs::exists(old_module.string()+".re4-controls.previous"));
        put(old_module,legacy);
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,{}),"Conflict"));
        const re4::KnownInstallFile wrong_index[]={{re4::GameTarget::RE4,2,256,0xE92A3B302C25C51Dull}};
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,{wrong_index,1}),"Conflict"));
        const re4::KnownInstallFile wrong_game[]={{re4::GameTarget::RE6,0,256,0xE92A3B302C25C51Dull}};
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,{wrong_game,1}),"Conflict"));
        // A matching legacy artifact cannot override absent/malformed ownership.
        const auto receipt=manual/"switch/re4-controls/installed-files.bin";
        std::string mismatch(32,'\0');mismatch.replace(0,8,"RE4INST1");mismatch[8]=1;
        put(receipt,mismatch);
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,catalog),"Conflict"));
        fs::remove(receipt);
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,catalog),"Conflict"));
        put(receipt,"invalid receipt");
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,catalog),"Conflict"));
        assert(std::strstr(re4::install_overlay_only(payload.string().c_str(),manual.string().c_str()),"Installed"));
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,catalog),"Installed"));
        assert(fs::exists(old_module.string()+".re4-controls.previous"));
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str()),"Installed"));
        // The repaired receipt must own the new module for the next update.
        auto next=nso;next[32]='N';put(payload/"subsdk9",next);
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),manual.string().c_str(),re4::GameTarget::RE4,{}),"Installed"));
        put(payload/"subsdk9",nso);
    }
    const auto game = sd / "atmosphere/contents/010099A00BC1E000/exefs";
    put(game / "main.npdm", "existing other mod");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(), sd.string().c_str()), "Conflict"));
    assert(!fs::exists(game / "subsdk9")); // Preflight rejects before publishing any file.
    fs::remove(game / "main.npdm");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(), sd.string().c_str()), "Installed"));
    assert(std::strstr(re4::install_game_files(payload.string().c_str(), sd.string().c_str()), "Installed"));
    nso[32] = 'B'; put(payload / "subsdk9", nso);
    assert(std::strstr(re4::install_game_files(payload.string().c_str(), sd.string().c_str()), "Installed"));
    assert(fs::exists(game / "subsdk9.re4-controls.previous"));
    // An unrelated modification after our deployment must not be overwritten.
    put(game / "main.npdm", "changed by another tool");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(), sd.string().c_str()), "Conflict"));
    assert(std::strstr(re4::disable_game_files(sd.string().c_str()), "Conflict"));
    assert(fs::exists(game / "subsdk9")); // Do not partially disable a conflicting installation.
    put(game / "main.npdm", npdm);
    assert(std::strstr(re4::disable_game_files(sd.string().c_str()), "Disabled"));
    ovl[32] = 'C'; put(payload / "re4-controls.ovl", ovl);
    assert(std::strstr(re4::install_overlay_only(payload.string().c_str(), sd.string().c_str()), "Installed"));
    assert(!fs::exists(game / "subsdk9") && !fs::exists(game / "main.npdm"));
    assert(fs::exists(sd / "switch/.overlays/re4-controls.ovl"));
    std::string loader(256, '\0'); loader.replace(0, 4, "PFS0");
    put(payload / "nx-ovlloader.nsp", loader); put(payload / "boot2.flag", "");
    const auto menu = sd / "switch/.overlays/ovlmenu.ovl";
    put(menu, "another overlay menu");
    assert(std::strstr(re4::install_overlay_loader(payload.string().c_str(), sd.string().c_str()), "Conflict"));
    assert(!fs::exists(sd / "atmosphere/contents/420000000007E51A/exefs.nsp"));
    fs::remove(menu);
    assert(std::strstr(re4::install_overlay_loader(payload.string().c_str(), sd.string().c_str()), "Installed"));
    assert(fs::exists(sd / "atmosphere/contents/420000000007E51A/flags/boot2.flag"));
    assert(std::strstr(re4::disable_overlay_loader(sd.string().c_str()), "Disabled"));
    assert(!fs::exists(game / "subsdk9") && !fs::exists(game / "main.npdm"));
    assert(fs::exists(game / "subsdk9.re4-controls.disabled"));
    assert(std::strstr(re4::disable_game_files(sd.string().c_str()), "Disabled"));
    assert(!fs::exists(sd / "switch/.overlays/re4-controls.ovl"));
    unsigned disabled_copies = 0;
    for (const auto& entry : fs::directory_iterator(sd / "switch/.overlays"))
        if (entry.path().filename().string().find("re4-controls.ovl.re4-controls.disabled") == 0) ++disabled_copies;
    assert(disabled_copies == 2); // Updated overlay gets its own backup, preserving the first.
    const auto re6 = sd / "atmosphere/contents/01002A000CD48000/exefs";
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"missing"));
    assert(!fs::exists(re6 / "subsdk9"));
    put(payload / "re6-subsdk9",nso);put(payload / "re6-main.npdm",npdm);
    put(payload / "re6-controls.ovl",ovl);
    // The normal bundle needs only controls, module and NPDM; telemetry is absent.
    put(re6 / "main.npdm","other RE6 mod");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"Conflict"));
    assert(!fs::exists(re6 / "subsdk9"));fs::remove(re6 / "main.npdm");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"Installed RE6 module"));
    const auto telemetry=sd / "switch/.overlays/re6-telemetry.ovl";
    const auto controls=sd / "switch/.overlays/re6-controls.ovl";
    const auto receipt6=sd / "switch/re4-controls/installed-re6-files.bin";
    assert(fs::exists(re6 / "subsdk9")&&!fs::exists(telemetry)&&fs::exists(controls));
    assert(get(receipt6).size()==32&&get(receipt6).substr(0,8)=="RE6INST3");
    assert(!fs::exists(game / "subsdk9")&&!fs::exists(sd / "switch/.overlays/re4-controls.ovl"));
    auto telemetry_bytes=ovl;telemetry_bytes[33]='T';
    auto old_controls=ovl;old_controls[33]='O';
    for(const auto* tag:{"RE6INST1","RE6INST2"}) {
        put(re6/"subsdk9",nso);put(re6/"main.npdm",npdm);put(telemetry,telemetry_bytes);
        const auto legacy=old_re6_receipt(tag,nso,npdm,telemetry_bytes,old_controls);
        put(receipt6,legacy);
        if(!std::strcmp(tag,"RE6INST1")) {
            // Old slot 2 owns telemetry, never an unrelated controls overlay.
            put(controls,telemetry_bytes);
            assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"Conflict"));
            assert(get(telemetry)==telemetry_bytes&&get(receipt6)==legacy);
            fs::remove(controls);
        } else put(controls,old_controls);
        put(sd/"switch/re4-controls/re6-controls.cfg","keep settings");
        put(sd/"switch/re4-controls/captures/session.csv","keep capture");
        put(sd/"switch/.overlays/other.ovl","keep other overlay");
        // A late receipt failure restores every member, including retired telemetry.
        fs::create_directories(receipt6.string()+".incoming");
        put(fs::path(receipt6.string()+".incoming")/"blocker","block");
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"rolled back"));
        assert(get(receipt6)==legacy&&get(telemetry)==telemetry_bytes);
        assert(get(re6/"subsdk9")==nso&&get(re6/"main.npdm")==npdm);
        if(!std::strcmp(tag,"RE6INST2"))assert(get(controls)==old_controls);
        else assert(!fs::exists(controls));
        fs::remove_all(receipt6.string()+".incoming");
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"Installed"));
        assert(!fs::exists(telemetry)&&get(controls)==ovl);
        assert(get(telemetry.string()+".re4-controls.disabled")==telemetry_bytes);
        assert(get(receipt6).size()==32&&get(receipt6).substr(0,8)=="RE6INST3");
        assert(get(sd/"switch/re4-controls/re6-controls.cfg")=="keep settings");
        assert(get(sd/"switch/re4-controls/captures/session.csv")=="keep capture");
        assert(get(sd/"switch/.overlays/other.ovl")=="keep other overlay");
    }
    {
        // Startup cleanup followed by overlay-only update preserves the old module
        // proof, so a later full RE6 update can still replace those exact files.
        const auto overlay_sd=root/"re6-overlay-migration";
        const auto mod=overlay_sd/"atmosphere/contents/01002A000CD48000/exefs/subsdk9";
        const auto meta=mod.parent_path()/"main.npdm";
        const auto ctl=overlay_sd/"switch/.overlays/re6-controls.ovl";
        const auto tel=overlay_sd/"switch/.overlays/re6-telemetry.ovl";
        const auto rec=overlay_sd/"switch/re4-controls/installed-re6-files.bin";
        auto old_nso=nso;old_nso[44]='L';auto old_npdm=npdm;old_npdm[44]='L';
        put(mod,old_nso);put(meta,old_npdm);put(ctl,old_controls);put(tel,telemetry_bytes);
        put(rec,old_re6_receipt("RE6INST2",old_nso,old_npdm,telemetry_bytes,old_controls));
        assert(std::strstr(re4::retire_telemetry_overlay(overlay_sd.string().c_str()),"Disabled"));
        assert(std::strstr(re4::install_overlay_only(payload.string().c_str(),overlay_sd.string().c_str(),re4::GameTarget::RE6),"Installed"));
        assert(get(mod)==old_nso&&get(meta)==old_npdm&&get(ctl)==ovl&&!fs::exists(tel));
        assert(get(rec).size()==32&&get(rec).substr(0,8)=="RE6INST3");
        assert(std::strstr(re4::install_game_files(payload.string().c_str(),overlay_sd.string().c_str(),re4::GameTarget::RE6),"Installed"));
        assert(get(mod)==nso&&get(meta)==npdm);
        auto newer_controls=ovl;newer_controls[45]='N';put(payload/"re6-controls.ovl",newer_controls);
        assert(std::strstr(re4::install_overlay_only(payload.string().c_str(),overlay_sd.string().c_str(),re4::GameTarget::RE6),"Installed"));
        assert(get(ctl)==newer_controls);
        assert(std::strstr(re4::disable_game_files(overlay_sd.string().c_str(),re4::GameTarget::RE6),"Disabled"));
        assert(!fs::exists(mod)&&!fs::exists(meta)&&!fs::exists(ctl));
        put(payload/"re6-controls.ovl",ovl);
    }
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),static_cast<re4::GameTarget>(99)),"Invalid"));
    assert(std::strstr(re4::disable_game_files(sd.string().c_str(),re4::GameTarget::RE6),"Disabled RE6"));
    assert(!fs::exists(re6/"subsdk9")&&!fs::exists(controls));
    assert(std::strstr(re4::install_overlay_only(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"Installed RE6 overlay"));
    assert(!fs::exists(re6/"subsdk9")&&!fs::exists(telemetry)&&fs::exists(controls));
    const auto re5 = sd / "atmosphere/contents/010018100CD46000/exefs";
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE5),"missing"));
    put(payload / "re5-subsdk9",nso);put(payload / "re5-main.npdm",npdm);put(payload / "re5-controls.ovl",ovl);
    put(sd / "switch/re4-controls/re5-controls.cfg","RE5 settings preserved");
    put(re5 / "main.npdm","foreign RE5 mod");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE5),"Conflict"));
    assert(!fs::exists(re5 / "subsdk9"));fs::remove(re5 / "main.npdm");
    // Receipt publication failure must remove every newly installed paired file.
    const auto receipt5=sd / "switch/re4-controls/installed-re5-files.bin";
    fs::create_directories(receipt5);put(receipt5 / "blocker","block");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE5),"rolled back"));
    assert(!fs::exists(re5 / "subsdk9")&&!fs::exists(re5 / "main.npdm"));
    fs::remove_all(receipt5);
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE5),"Installed RE5"));
    assert(fs::exists(re5 / "subsdk9")&&fs::exists(sd / "switch/.overlays/re5-controls.ovl"));
    assert(fs::file_size(receipt5)==32);
    assert(std::strstr(re4::disable_game_files(sd.string().c_str(),re4::GameTarget::RE5),"Disabled RE5"));
    assert(!fs::exists(re5 / "subsdk9"));
    assert(fs::exists(sd / "switch/re4-controls/re5-controls.cfg"));
    assert(fs::exists(sd / "switch/.overlays/re6-controls.ovl"));
    assert(std::strstr(re4::install_overlay_only(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE5),"Installed RE5 overlay"));
    assert(!fs::exists(re5 / "subsdk9")&&fs::exists(sd / "switch/.overlays/re5-controls.ovl"));
    fs::remove_all(root);
    std::puts("installer: missing payload, foreign conflicts, idempotence and own update PASS");
}
