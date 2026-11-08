#pragma once
#include <cstdint>
#include "re4/launch_mode.hpp"
class GameLink;
namespace overlay_debug {
enum class State {Off,MissingKey,ServicesUnavailable,SocketsUnavailable,Starting,Listening,Failed};
void start(const char* token_path="r4ovl:/switch/re4-controls/token.txt");
// Default Normal does not inspect the credential or initialize diagnostic IO.
void set_mode(re4::LaunchMode mode,const char* token_path="r4ovl:/switch/re4-controls/token.txt");
re4::LaunchMode mode();
// Worker borrows this link's debug session; call before start, stop before closing it.
void share_link(const GameLink* owner);
void stop();
State state();
std::uint16_t port();
}
