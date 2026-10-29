#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace re4 {
constexpr const char* kUpdateRepository="EscHelpNew/re4-controll-mod";
constexpr const char* kUpdateBase="https://github.com/EscHelpNew/re4-controll-mod/releases/latest/download/";
constexpr std::uint32_t kUpdateMaxBytes=64u*1024u*1024u;
struct ReleaseInfo {
    std::string version;
    std::uint32_t bytes=0;
    std::array<unsigned char,32> sha256{};
};
bool parse_release_manifest(const std::string&,ReleaseInfo&);
bool newer_version(const std::string& candidate,const std::string& installed);
using UpdateHash=int(*)(const char*,unsigned char*);
struct UpdateFileOps {virtual ~UpdateFileOps()=default;virtual int move(const char*,const char*);};
bool publish_update(const std::string& root,const ReleaseInfo&,UpdateHash,UpdateFileOps&,std::string& detail);
bool bootstrap_self_images(const std::string& source,const std::string& root,UpdateHash,UpdateFileOps&,std::string& detail);
int update_hash_file(const char*,unsigned char*);
enum class UpdateStage {Idle,Checking,Available,Current,Downloading,Installing,Installed,Failed,Cancelled};
struct UpdateSnapshot {
    UpdateStage stage=UpdateStage::Idle;
    std::string version,detail;
    std::uint32_t received=0,total=0;
};
class UpdateWorker {
    struct Impl;Impl* impl_=nullptr;
public:
    UpdateWorker();~UpdateWorker();
    UpdateWorker(const UpdateWorker&)=delete;UpdateWorker& operator=(const UpdateWorker&)=delete;
    bool check();bool install();void cancel();void close();bool busy()const;
    UpdateSnapshot snapshot()const;
};
}
