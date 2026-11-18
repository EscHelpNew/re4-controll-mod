#pragma once
#include <cstdint>
#include <string>
namespace re4 {
enum class LocalUpdateState { Off,Starting,Listening,Receiving,Stopping,Failed,Stored };
struct LocalUpdateSnapshot {
    LocalUpdateState state=LocalUpdateState::Off;
    std::string address,detail;
    std::uint32_t received=0,total=0;
};
}
