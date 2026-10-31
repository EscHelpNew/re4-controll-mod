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
constexpr const char* re6_names[] = {"re6-subsdk9", "re6-main.npdm", "re6-telemetry.ovl", "re6-controls.ovl"};
constexpr const char* re6_destinations[] = {
    "/atmosphere/contents/01002A000CD48000/exefs/subsdk9",
    "/atmosphere/contents/01002A000CD48000/exefs/main.npdm",
    "/switch/.overlays/re6-telemetry.ovl", "/switch/.overlays/re6-controls.ovl"};
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
}
static const char* install_bundle(const char* payload_root, const char* sd_root, bool loader, bool only_overlay = false, GameTarget game=GameTarget::RE4) {
    if(game!=GameTarget::RE4&&game!=GameTarget::RE6)return "Invalid game target";
    if (!payload_root || !sd_root) return "Payload missing";
    const bool re6=game==GameTarget::RE6&&!loader;
    const auto* file_names=loader?loader_names:re6?re6_names:names;
    const auto* file_destinations=loader?loader_destinations:re6?re6_destinations:destinations;
    const auto receipt_tag=re6?"RE6INST2":"RE4INST1";
    const unsigned count=re6?4:3;
    std::array<Bytes, 4> payload;
    std::array<std::string, 4> paths;
    Bytes receipt;
    const auto receipt_path = std::string(sd_root) + (loader ? "/switch/re4-controls/installed-loader.bin" : re6?"/switch/re4-controls/installed-re6-files.bin":"/switch/re4-controls/installed-files.bin");
    const bool had_receipt=read(receipt_path,receipt);
    const bool ours=had_receipt&&((receipt.size()==8+count*8&&!std::memcmp(receipt.data(),receipt_tag,8))
        ||(re6&&receipt.size()==32&&!std::memcmp(receipt.data(),"RE6INST1",8)));
    const unsigned owned_count=ours?unsigned((receipt.size()-8)/8):0;
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
            if (i<owned_count) std::memcpy(&expected, receipt.data() + 8 + 8 * i, 8);
            if (i>=owned_count || hash(current) != expected) return "Conflict: existing files belong to another mod";
        }
    }
    for (unsigned i = 0; i < count; ++i)
        if ((!only_overlay || i >= 2) && !publish(paths[i], payload[i])) return "SD write failed; close the target game until installation succeeds";
    if (!ours) receipt.assign(8+count*8, 0);
    else receipt.resize(8+count*8,0);
    std::memcpy(receipt.data(), receipt_tag, 8);
    for (unsigned i = 0; i < count; ++i) {
        if (only_overlay && i < 2) continue;
        const auto digest = hash(payload[i]);
        std::memcpy(receipt.data() + 8 + 8 * i, &digest, 8);
    }
    if (!publish(receipt_path, receipt)) return "Files installed; receipt write failed";
    if(re6)return only_overlay?"Installed RE6 overlay. Open the existing Uberhand menu.":"Installed RE6 module. Restart RE6.";
    return loader ? "Installed overlay loader. Reboot CFW to enable the menu." : only_overlay
        ? "Installed RE4 overlay. Open the existing Uberhand menu."
        : "Installed RE4 Controls. Restart RE4.";
}
static const char* disable_bundle(const char* sd_root, bool loader, GameTarget game=GameTarget::RE4) {
    if(game!=GameTarget::RE4&&game!=GameTarget::RE6)return "Invalid game target";
    if (!sd_root) return "Conflict: no SD root";
    const bool re6=game==GameTarget::RE6&&!loader;
    Bytes receipt;
    if (!read(std::string(sd_root) + (loader ? "/switch/re4-controls/installed-loader.bin" : re6?"/switch/re4-controls/installed-re6-files.bin":"/switch/re4-controls/installed-files.bin"), receipt)
        || !((receipt.size()==32&&!std::memcmp(receipt.data(),re6?"RE6INST1":"RE4INST1",8))
            ||(re6&&receipt.size()==40&&!std::memcmp(receipt.data(),"RE6INST2",8))))
        return "Conflict: installation receipt unavailable";
    const unsigned count=unsigned((receipt.size()-8)/8);
    std::array<std::string, 4> paths;
    std::array<std::string, 4> backups;
    for (unsigned i = 0; i < count; ++i) {
        paths[i] = std::string(sd_root) + (loader ? loader_destinations[i] : re6?re6_destinations[i]:destinations[i]);
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
    for (unsigned i = 0; i < count; ++i) {
        const auto& path = paths[i];
        if (!exists(path)) continue;
        const auto& backup = backups[i];
        if (exists(backup)) {
            if (std::remove(path.c_str())) return "SD error while disabling patch";
        } else if (std::rename(path.c_str(), backup.c_str())) return "SD error while disabling patch";
    }
    return loader ? "Disabled overlay loader. Reboot CFW to stop the menu." : re6?"Disabled RE6 mod. Restart RE6.":"Disabled RE4 patch. Original game files will load at next start.";
}
const char* install_game_files(const char* payload, const char* sd, GameTarget game) { return install_bundle(payload, sd, false, false, game); }
const char* disable_game_files(const char* sd, GameTarget game) { return disable_bundle(sd, false, game); }
const char* install_overlay_loader(const char* payload, const char* sd) { return install_bundle(payload, sd, true); }
const char* install_overlay_only(const char* payload, const char* sd, GameTarget game) { return install_bundle(payload, sd, false, true, game); }
const char* disable_overlay_loader(const char* sd) { return disable_bundle(sd, true); }
}
