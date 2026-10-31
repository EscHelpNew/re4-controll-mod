#pragma once
#include <cstddef>
#include <cstdint>

namespace re6 {
inline constexpr char kTelemetryLogRoot[]="sdmc:/switch/re4-controls/telemetry";
inline constexpr unsigned kTelemetryLogPort=28286;
inline constexpr std::uint64_t kMaxTelemetryLogBytes=64ull*1024*1024;
inline constexpr std::size_t kMaxTelemetryLogEntries=1024;
// The caller owns BSD initialization and must stop before socketExit().
// Invalid/missing local credentials disable this server. Never embeds a token.
bool start_log_server(const char* token);
void stop_log_server();
bool load_log_token(const char* path,char (&token)[65]);

namespace detail {
// Portable session engine. The network adapter bounds each callback with a
// deadline. Callbacks return short counts, zero on EOF, or negative on failure.
struct LogIo {
 void* context;
 int (*read)(void*,void*,std::size_t);
 int (*write)(void*,const void*,std::size_t);
 bool (*cancelled)(void*);
};
void serve_log_session(const char* root,const char* token,const LogIo& io);
}
}
