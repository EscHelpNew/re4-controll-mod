#include "re4/settings_file.hpp"
#include <cstdio>
#include <string>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace re4 {
namespace {
bool read(const char* path, Settings& out) {
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    std::uint8_t bytes[kSettingsBytes + 1];
    const auto size = std::fread(bytes, 1, sizeof(bytes), file);
    const bool good = !std::ferror(file);
    std::fclose(file);
    return good && decode_settings(bytes, size, out);
}
}
bool load_settings_file(const char* path, Settings& out) {
    if (!path) return false;
    return read(path, out) || read((std::string(path) + ".previous").c_str(), out);
}
bool save_settings_file(const char* path, const Settings& settings) {
    std::uint8_t bytes[kSettingsBytes];
    if (!path || !encode_settings(settings, bytes, sizeof(bytes))) return false;
    const auto incoming = std::string(path) + ".incoming";
    const auto previous = std::string(path) + ".previous";
    FILE* file = std::fopen(incoming.c_str(), "wb");
    if (!file) return false;
    const bool written = std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
    bool synced = std::fflush(file) == 0;
#ifdef _WIN32
    if (synced) synced = _commit(_fileno(file)) == 0;
#else
    if (synced) synced = fsync(fileno(file)) == 0;
#endif
    const bool closed = std::fclose(file) == 0;
    if (!written || !synced || !closed) return false;
    struct stat original{};
    const bool had_original = stat(path, &original) == 0;
    Settings current;
    const bool rotated = had_original && read(path, current);
    if (rotated) {
        struct stat backup{};
        if (stat(previous.c_str(), &backup) == 0 && std::remove(previous.c_str())) return false;
        if (std::rename(path, previous.c_str())) return false;
    } else if (had_original && std::remove(path)) {
        // Retain the known-good recovery copy when the primary is corrupt.
        return false;
    }
    if (std::rename(incoming.c_str(), path) == 0) return true;
    if (rotated) std::rename(previous.c_str(), path);
    return false;
}
}
