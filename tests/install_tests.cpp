#include "re4/install.hpp"
#include "re4/game_target.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
namespace fs = std::filesystem;
void put(const fs::path& p, const std::string& bytes) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary).write(bytes.data(), bytes.size());
}
int main() {
    const fs::path root = "build/install-test";
    fs::remove_all(root);
    const auto payload = root / "payload", sd = root / "sd";
    fs::create_directories(sd);
    assert(std::strstr(re4::install_game_files(payload.string().c_str(), sd.string().c_str()), "missing"));
    std::string nso(256, '\0'), npdm(256, '\0'), ovl(256, '\0');
    nso.replace(0, 4, "NSO0"); npdm.replace(0, 4, "META"); ovl.replace(16, 4, "NRO0");
    put(payload / "subsdk9", nso); put(payload / "main.npdm", npdm); put(payload / "re4-controls.ovl", ovl);
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
    put(payload / "re6-subsdk9",nso);put(payload / "re6-main.npdm",npdm);put(payload / "re6-telemetry.ovl",ovl);
    put(re6 / "main.npdm","other RE6 mod");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"Conflict"));
    assert(!fs::exists(re6 / "subsdk9"));fs::remove(re6 / "main.npdm");
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"Installed RE6 module"));
    assert(fs::exists(re6 / "subsdk9")&&fs::exists(sd / "switch/.overlays/re6-telemetry.ovl"));
    assert(!fs::exists(game / "subsdk9")&&!fs::exists(sd / "switch/.overlays/re4-controls.ovl"));
    const auto receipt6=sd / "switch/re4-controls/installed-re6-files.bin";
    assert(fs::exists(receipt6));
    {std::ifstream f(receipt6,std::ios::binary);char tag[8]{};f.read(tag,8);assert(!std::memcmp(tag,"RE6INST1",8));}
    assert(std::strstr(re4::install_game_files(payload.string().c_str(),sd.string().c_str(),static_cast<re4::GameTarget>(99)),"Invalid"));
    assert(std::strstr(re4::disable_game_files(sd.string().c_str(),re4::GameTarget::RE6),"Disabled RE6"));
    assert(!fs::exists(re6 / "subsdk9"));
    assert(std::strstr(re4::install_overlay_only(payload.string().c_str(),sd.string().c_str(),re4::GameTarget::RE6),"Installed RE6 overlay"));
    assert(!fs::exists(re6 / "subsdk9")&&fs::exists(sd / "switch/.overlays/re6-telemetry.ovl"));
    fs::remove_all(root);
    std::puts("installer: missing payload, foreign conflicts, idempotence and own update PASS");
}
