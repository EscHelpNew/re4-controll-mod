#include "re4/launch_mode.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
int main(){
    using re4::LaunchMode;
    const auto root=std::filesystem::temp_directory_path()/
        ("re4-launch-mode-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(root));const auto path=(root/"launch-mode.bin").string();
    LaunchMode mode=LaunchMode::Diagnostics;
    assert(!re4::load_launch_mode(path.c_str(),mode)&&mode==LaunchMode::Normal);
    assert(std::filesystem::is_empty(root)); // Loading defaults never creates files.
    assert(re4::save_launch_mode(path.c_str(),LaunchMode::Diagnostics));
    assert(re4::load_launch_mode(path.c_str(),mode)&&mode==LaunchMode::Diagnostics);
    assert(re4::save_launch_mode(path.c_str(),LaunchMode::Normal));
    assert(re4::load_launch_mode(path.c_str(),mode)&&mode==LaunchMode::Normal);
    for(const auto bytes:{"","diagnostics","R4LM\1\2","R4LM\1\1\0\0EXTRA"}){
        {std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<bytes;}
        mode=LaunchMode::Diagnostics;assert(!re4::load_launch_mode(path.c_str(),mode)&&mode==LaunchMode::Normal);
    }
    assert(!re4::save_launch_mode(path.c_str(),static_cast<LaunchMode>(255)));
    assert(!re4::save_launch_mode(nullptr,LaunchMode::Diagnostics));
    assert(!re4::save_launch_mode((root/"missing"/"mode.bin").string().c_str(),LaunchMode::Diagnostics));
    assert(!re4::save_launch_mode(root.string().c_str(),LaunchMode::Normal));
    std::filesystem::remove(path);mode=LaunchMode::Diagnostics;
    assert(!re4::load_launch_mode(path.c_str(),mode)&&mode==LaunchMode::Normal); // Never resurrect the Diagnostic backup.
    for(const auto& file:std::filesystem::directory_iterator(root))std::filesystem::remove(file.path());
    std::filesystem::remove(root);
    std::puts("launch mode preference: Normal default, strict format, persistence, failure and no backup resurrection PASS");
}
