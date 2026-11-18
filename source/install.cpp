#include "re4/install.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace re4 {
namespace {
using Bytes = std::vector<unsigned char>;
constexpr const char* names[] = {"subsdk9", "main.npdm", "re4-controls.ovl"};
constexpr const char* destinations[] = {
    "/atmosphere/contents/010099A00BC1E000/exefs/subsdk9",
    "/atmosphere/contents/010099A00BC1E000/exefs/main.npdm",
    "/switch/.overlays/re4-controls.ovl"};
constexpr const char* re6_names[] = {"re6-subsdk9", "re6-main.npdm", "re6-controls.ovl"};
constexpr const char* re6_destinations[] = {
    "/atmosphere/contents/01002A000CD48000/exefs/subsdk9",
    "/atmosphere/contents/01002A000CD48000/exefs/main.npdm",
    "/switch/.overlays/re6-controls.ovl"};
constexpr const char* old_re6_destinations[] = {
    re6_destinations[0], re6_destinations[1],
    "/switch/.overlays/re6-telemetry.ovl", re6_destinations[2]};
constexpr const char* re5_names[] = {"re5-subsdk9", "re5-main.npdm", "re5-controls.ovl"};
constexpr const char* re5_destinations[] = {
    "/atmosphere/contents/010018100CD46000/exefs/subsdk9",
    "/atmosphere/contents/010018100CD46000/exefs/main.npdm",
    "/switch/.overlays/re5-controls.ovl"};
constexpr const char* loader_names[] = {"nx-ovlloader.nsp", "re4-controls.ovl", "boot2.flag"};
constexpr const char* loader_destinations[] = {
    "/atmosphere/contents/420000000007E51A/exefs.nsp",
    "/switch/.overlays/ovlmenu.ovl",
    "/atmosphere/contents/420000000007E51A/flags/boot2.flag"};
bool exists(const std::string& path) {
    struct stat st{};
    return !stat(path.c_str(), &st);
}
bool read(const std::string& path, Bytes& bytes) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    struct stat st{};
    const bool sized = !fstat(fileno(file), &st) && st.st_size >= 0 && st.st_size <= 4 * 1024 * 1024;
    if (sized) bytes.resize(static_cast<size_t>(st.st_size));
    const bool ok = sized && std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size() && !std::ferror(file);
    return std::fclose(file) == 0 && ok;
}
std::uint64_t hash(const Bytes& bytes) {
    std::uint64_t value = 14695981039346656037ull;
    for (const auto byte : bytes) value = (value ^ byte) * 1099511628211ull;
    return value;
}
bool directories(const std::string& path) {
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i] != '/') continue;
        const auto part = path.substr(0, i);
        if (part.back() == ':') continue;
#ifdef _WIN32
        const int result = _mkdir(part.c_str());
#else
        const int result = mkdir(part.c_str(), 0777);
#endif
        if (result && errno != EEXIST) return false;
    }
    return true;
}
bool write(const std::string& path, const Bytes& bytes) {
    if (!directories(path)) return false;
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    if (std::fflush(file)) ok = false;
#ifdef _WIN32
    if (_commit(_fileno(file))) ok = false;
#else
    if (fsync(fileno(file))) ok = false;
#endif
    if (std::fclose(file)) ok = false;
    return ok;
}
bool publish(const std::string& path, const Bytes& bytes) {
    Bytes old;
    const bool had_old = exists(path);
    if (had_old && !read(path, old)) return false;
    if (had_old && old == bytes) return true;
    const auto incoming = path + ".incoming";
    Bytes verified;
    if (!write(incoming, bytes) || !read(incoming, verified) || verified != bytes) return false;
    const auto backup = path + ".re4-controls.previous";
    if (had_old && !exists(backup) && !write(backup, old)) return false;
    if (had_old && std::remove(path.c_str())) return false;
    if (!std::rename(incoming.c_str(), path.c_str())) return true;
    if (had_old) write(path, old);
    return false;
}

bool re6_receipt(const Bytes& receipt, bool legacy_only = false) {
    return (receipt.size()==40&&!std::memcmp(receipt.data(),"RE6INST2",8))
        || (receipt.size()==32&&(!std::memcmp(receipt.data(),"RE6INST1",8)
            || (!legacy_only&&!std::memcmp(receipt.data(),"RE6INST3",8))));
}
bool legacy_re6_receipt(const Bytes& receipt) { return re6_receipt(receipt, true); }
// A retirement is reversible and does not replace an earlier different backup.
struct TelemetryRetirement {
    std::string path, backup;
    Bytes original;
    bool present=false, changed=false, had_backup=false;
    const char* prepare(const char* sd_root, const Bytes& receipt) {
        path=std::string(sd_root)+old_re6_destinations[2];
        if (!exists(path)) return nullptr;
        if (!legacy_re6_receipt(receipt)) return "Conflict: telemetry ownership receipt unavailable";
        std::uint64_t expected=0;
        std::memcpy(&expected,receipt.data()+24,8);
        if (!expected) return "Conflict: telemetry ownership receipt unavailable";
        if (!read(path,original)||hash(original)!=expected)
            return "Conflict: telemetry overlay changed outside RE4 Controls";
        backup=path+".re4-controls.disabled";
        Bytes disabled;
        if (exists(backup)&&(!read(backup,disabled)||disabled!=original)) {
            char suffix[32];
            std::snprintf(suffix,sizeof(suffix),"-%016llx",(unsigned long long)expected);
            backup+=suffix;
            if (exists(backup)&&(!read(backup,disabled)||disabled!=original))
                return "Conflict: disabled telemetry backup differs";
        }
        had_backup=exists(backup);
        present=true;
        return nullptr;
    }
    bool retire() {
        if (!present) return true;
        const bool ok=had_backup ? !std::remove(path.c_str()) : !std::rename(path.c_str(),backup.c_str());
        changed=ok;
        return ok;
    }
    bool restore() {
        if (!changed) return true;
        if (!write(path,original)) return false;
        return had_backup || !std::remove(backup.c_str());
    }
};
}
InstallCatalog legacy_install_catalog(){
    // Own v0.16.1 subsdk9, verified against the retained release ZIP. A manual
    // deployment followed by overlay-only install left its receipt slot zero.
    static constexpr KnownInstallFile files[]={{GameTarget::RE4,0,136140,0xF4611F9AA197EBE2ull}};
    return {files,sizeof(files)/sizeof(files[0])};
}
static const char* install_bundle(const char* payload_root, const char* sd_root, bool loader, bool only_overlay = false, GameTarget game=GameTarget::RE4,InstallCatalog known={}) {
    if(game!=GameTarget::RE4&&game!=GameTarget::RE5&&game!=GameTarget::RE6)return "Invalid game target";
    if (!payload_root || !sd_root) return "Payload missing";
    const bool re6=game==GameTarget::RE6&&!loader;
    const bool re5=game==GameTarget::RE5&&!loader;
    const auto* file_names=loader?loader_names:re5?re5_names:re6?re6_names:names;
    const auto* file_destinations=loader?loader_destinations:re5?re5_destinations:re6?re6_destinations:destinations;
    const auto receipt_tag=re5?"RE5INST1":re6?"RE6INST3":"RE4INST1";
    const unsigned count=3;
    std::array<Bytes, 4> payload;
    std::array<std::string, 4> paths;
    Bytes receipt;
    const auto receipt_path = std::string(sd_root) + (loader ? "/switch/re4-controls/installed-loader.bin" : re5?"/switch/re4-controls/installed-re5-files.bin":re6?"/switch/re4-controls/installed-re6-files.bin":"/switch/re4-controls/installed-files.bin");
    const bool receipt_was_absent=!exists(receipt_path);
    const bool had_receipt=read(receipt_path,receipt);
    const Bytes previous_receipt=receipt;
    const bool ours=had_receipt&&((receipt.size()==8+count*8&&!std::memcmp(receipt.data(),receipt_tag,8))
        ||(re6&&re6_receipt(receipt)));
    const unsigned owned_count=ours?unsigned((receipt.size()-8)/8):0;
    const bool old_re6=re6&&ours&&legacy_re6_receipt(receipt);
    TelemetryRetirement telemetry;
    if (re6) { if (const auto error=telemetry.prepare(sd_root,receipt)) return error; }
    for (unsigned i = 0; i < count; ++i) {
        if (!read(std::string(payload_root) + '/' + file_names[i], payload[i])) return "Payload missing";
        const size_t offset = loader ? (i == 1 ? 16 : 0) : (i >= 2 ? 16 : 0);
        const char* magic = loader ? (i == 0 ? "PFS0" : "NRO0") : (i == 0 ? "NSO0" : i == 1 ? "META" : "NRO0");
        if (loader && i == 2) {
            if (!payload[i].empty()) return "Invalid boot flag payload";
        } else if (payload[i].size() < 128 || std::memcmp(payload[i].data() + offset, magic, 4)) return "Invalid payload";
        paths[i] = std::string(sd_root) + file_destinations[i];
        if ((!only_overlay || i >= 2) && exists(paths[i])) {
            Bytes current;
            if (!read(paths[i], current)) return "Cannot read existing file";
            if (current == payload[i]) continue;
            std::uint64_t expected = 0;
            const unsigned receipt_index=old_re6&&i==2?3:i;
            if (receipt_index<owned_count) std::memcpy(&expected, receipt.data() + 8 + 8 * receipt_index, 8);
            if (receipt_index>=owned_count || hash(current) != expected) {
                bool legacy=false;
                if(!loader&&!only_overlay&&ours&&i<owned_count&&expected==0&&known.files){
                    const auto digest=hash(current);
                    for(std::size_t j=0;j<known.count;++j){
                        const auto& file=known.files[j];
                        if(file.game==game&&file.index==i&&file.bytes==current.size()&&file.digest==digest){legacy=true;break;}
                    }
                }
                if(!legacy)return "Conflict: existing files belong to another mod";
            }
        }
    }
    // Snapshot every selected destination before publishing any bundle member.
    std::array<Bytes, 4> previous;
    std::array<bool, 4> existed{};
    for (unsigned i = 0; i < count; ++i) {
        if (only_overlay && i < 2) continue;
        existed[i] = exists(paths[i]);
        if (existed[i] && !read(paths[i], previous[i])) return "Cannot read existing file";
    }
    const auto rollback = [&](bool restore_receipt = false) {
        bool ok = true;
        for (unsigned i = 0; i < count; ++i) {
            if (only_overlay && i < 2) continue;
            if (existed[i]) { if (!write(paths[i], previous[i])) ok = false; }
            else if (exists(paths[i]) && std::remove(paths[i].c_str())) ok = false;
            std::remove((paths[i] + ".incoming").c_str());
        }
        if (re6&&!telemetry.restore()) ok=false;
        if (restore_receipt) {
            if (had_receipt) {
                Bytes verified;
                if (!write(receipt_path,previous_receipt)||!read(receipt_path,verified)
                    ||verified!=previous_receipt) ok=false;
            } else if (receipt_was_absent) {
                if (exists(receipt_path)&&std::remove(receipt_path.c_str())) ok=false;
                if (exists(receipt_path)) ok=false;
            }
        }
        return ok;
    };
    for (unsigned i = 0; i < count; ++i) {
        if ((!only_overlay || i >= 2) && !publish(paths[i], payload[i])) {
            return rollback() ? "SD write failed; installation rolled back" : "SD write failed; rollback incomplete";
        }
    }
    if (re6&&!telemetry.retire())
        return rollback() ? "SD write failed; installation rolled back" : "SD write failed; rollback incomplete";
    // Preserve ownership of skipped module/NPDM slots during overlay-only migration.
    Bytes next_receipt(8+count*8,0);
    if (ours) for (unsigned i=0;i<2;++i)
        std::memcpy(next_receipt.data()+8+8*i,receipt.data()+8+8*i,8);
    receipt=next_receipt;
    std::memcpy(receipt.data(), receipt_tag, 8);
    for (unsigned i = 0; i < count; ++i) {
        if (only_overlay && i < 2) continue;
        const auto digest = hash(payload[i]);
        std::memcpy(receipt.data() + 8 + 8 * i, &digest, 8);
    }
    if (!publish(receipt_path, receipt)) {
        const bool restored = rollback(true);
        return restored ? "Receipt write failed; installation rolled back" : "Receipt write failed; rollback incomplete";
    }
    if(re5)return only_overlay?"Installed RE5 overlay. Open the existing Uberhand menu.":"Installed RE5 module. Restart RE5.";
    if(re6)return only_overlay?"Installed RE6 overlay. Open the existing Uberhand menu.":"Installed RE6 module. Restart RE6.";
    return loader ? "Installed overlay loader. Reboot CFW to enable the menu." : only_overlay
        ? "Installed RE4 overlay. Open the existing Uberhand menu."
        : "Installed RE4 Controls. Restart RE4.";
}
static const char* disable_bundle(const char* sd_root, bool loader, GameTarget game=GameTarget::RE4) {
    if(game!=GameTarget::RE4&&game!=GameTarget::RE5&&game!=GameTarget::RE6)return "Invalid game target";
    if (!sd_root) return "Conflict: no SD root";
    const bool re6=game==GameTarget::RE6&&!loader;
    const bool re5=game==GameTarget::RE5&&!loader;
    Bytes receipt;
    if (!read(std::string(sd_root) + (loader ? "/switch/re4-controls/installed-loader.bin" : re5?"/switch/re4-controls/installed-re5-files.bin":re6?"/switch/re4-controls/installed-re6-files.bin":"/switch/re4-controls/installed-files.bin"), receipt)
        || !((receipt.size()==32&&!std::memcmp(receipt.data(),re5?"RE5INST1":re6?"RE6INST1":"RE4INST1",8))
            ||(re6&&re6_receipt(receipt))))
        return "Conflict: installation receipt unavailable";
    const unsigned count=unsigned((receipt.size()-8)/8);
    const auto* re6_files=legacy_re6_receipt(receipt)?old_re6_destinations:re6_destinations;
    std::array<std::string, 4> paths;
    std::array<std::string, 4> backups;
    for (unsigned i = 0; i < count; ++i) {
        paths[i] = std::string(sd_root) + (loader ? loader_destinations[i] : re5?re5_destinations[i]:re6?re6_files[i]:destinations[i]);
        if (!exists(paths[i])) continue;
        Bytes current, disabled;
        std::uint64_t expected;
        std::memcpy(&expected, receipt.data() + 8 + 8 * i, 8);
        if (!read(paths[i], current) || hash(current) != expected)
            return "Conflict: file changed outside RE4 Controls";
        backups[i] = paths[i] + ".re4-controls.disabled";
        if (exists(backups[i]) && (!read(backups[i], disabled) || disabled != current)) {
            char suffix[32];
            std::snprintf(suffix, sizeof(suffix), "-%016llx", (unsigned long long)expected);
            backups[i] += suffix;
            if (exists(backups[i]) && (!read(backups[i], disabled) || disabled != current))
                return "Conflict: disabled backup differs";
        }
    }
    std::array<Bytes, 4> originals;
    std::array<bool, 4> present{};
    for (unsigned i = 0; i < count; ++i) {
        present[i] = exists(paths[i]);
        if (present[i] && !read(paths[i], originals[i])) return "Cannot read existing file";
    }
    for (unsigned i = 0; i < count; ++i) {
        const auto& path = paths[i];
        if (!present[i]) continue;
        const auto& backup = backups[i];
        const bool failed = exists(backup) ? std::remove(path.c_str()) != 0 : std::rename(path.c_str(), backup.c_str()) != 0;
        if (failed) {
            bool restored = true;
            for (unsigned j = 0; j < i; ++j)
                if (present[j] && !write(paths[j], originals[j])) restored = false;
            return restored ? "SD error while disabling; installation restored" : "SD error while disabling; rollback incomplete";
        }
    }
    return loader ? "Disabled overlay loader. Reboot CFW to stop the menu." : re5?"Disabled RE5 mod. Restart RE5.":re6?"Disabled RE6 mod. Restart RE6.":"Disabled RE4 patch. Original game files will load at next start.";
}
const char* retire_telemetry_overlay(const char* sd_root) {
    if (!sd_root) return "Conflict: no SD root";
    Bytes receipt;
    read(std::string(sd_root)+"/switch/re4-controls/installed-re6-files.bin",receipt);
    TelemetryRetirement telemetry;
    if (const auto error=telemetry.prepare(sd_root,receipt)) return error;
    if (!telemetry.retire()) return "SD write failed while retiring telemetry overlay";
    return telemetry.present?"Disabled telemetry overlay.":"Telemetry overlay already absent.";
}
const char* install_game_files(const char* payload, const char* sd, GameTarget game,InstallCatalog known) { return install_bundle(payload, sd, false, false, game,known); }
const char* disable_game_files(const char* sd, GameTarget game) { return disable_bundle(sd, false, game); }
const char* install_overlay_loader(const char* payload, const char* sd) { return install_bundle(payload, sd, true); }
const char* install_overlay_only(const char* payload, const char* sd, GameTarget game) { return install_bundle(payload, sd, false, true, game); }
const char* disable_overlay_loader(const char* sd) { return disable_bundle(sd, true); }
}
