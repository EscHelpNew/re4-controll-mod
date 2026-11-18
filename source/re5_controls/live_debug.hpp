#pragma once
#include <cstdint>
class Re5GameLink;
namespace re5_overlay_debug {
enum class State {Off,MissingKey,ServicesUnavailable,SocketsUnavailable,Starting,Listening,Failed};
// Explicit opt-in only. Disabled mode never reads credentials or opens sockets.
void set_enabled(bool enabled,const char* token_path="r5ovl:/switch/re4-controls/token.txt");
bool enabled();
// Set before enabling; stop/join before clearing or closing the owner.
void share_link(const Re5GameLink* owner);
void stop();
State state();
std::uint16_t port();
}
