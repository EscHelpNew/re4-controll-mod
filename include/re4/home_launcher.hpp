#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace re4 {
constexpr std::uint64_t kOwnLauncherId=0x0100E4C04E540000ull;
struct HomeContent {
    std::array<std::uint8_t,16> id{};
    std::array<std::uint8_t,32> hash{};
    std::uint8_t type=0;
    std::vector<std::uint8_t> data;
};
struct HomeBundle {
    std::uint64_t title=0;
    std::uint32_t version=0;
    std::vector<std::uint8_t> database;
    std::array<HomeContent,3> contents;
};
using HomeVerify=bool(*)(const std::uint8_t*,std::size_t,const std::uint8_t*);
bool parse_home_bundle(const std::uint8_t*,std::size_t,HomeVerify,HomeBundle&);
enum class HomeResult { Current,Installed,NeedsRefresh,Failed };
struct HomeBackend {
    virtual ~HomeBackend()=default;
    virtual bool begin()=0;
    virtual int current(const HomeBundle&)=0; // -1 error, 0 absent/old, 1 exact current bundle.
    virtual bool put(const HomeContent&)=0;
    virtual bool snapshot(const HomeBundle&)=0;
    virtual bool publish(const HomeBundle&)=0;
    virtual bool register_record(const HomeBundle&)=0;
    virtual bool refresh()=0;
    virtual void restore(const HomeBundle&)=0;
};
// Bundle must have passed parse_home_bundle before entering the transaction.
inline HomeResult ensure_home(const HomeBundle& bundle,HomeBackend& backend){
    if(bundle.title!=kOwnLauncherId || !backend.begin())return HomeResult::Failed;
    const int current=backend.current(bundle);
    if(current<0)return HomeResult::Failed;
    if(!current){
        for(const auto& content:bundle.contents)if(!backend.put(content))return HomeResult::Failed;
        if(!backend.snapshot(bundle))return HomeResult::Failed;
        if(!backend.publish(bundle) || !backend.register_record(bundle)){
            backend.restore(bundle);return HomeResult::Failed;
        }
    }else if(!backend.register_record(bundle))return HomeResult::Failed;
    if(!backend.refresh())return HomeResult::NeedsRefresh;
    return current?HomeResult::Current:HomeResult::Installed;
}
HomeResult install_own_home_launcher(const char* bundle_path,std::string& detail);
}
