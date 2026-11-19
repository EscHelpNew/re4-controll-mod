#pragma once
#include "local_update.hpp"
#include <cstddef>
#include <memory>
namespace re4 {
inline constexpr std::size_t kLocalUpdateReportCapacity=1024*1024;
class LocalUpdateServer {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    using ReportFn=std::size_t(*)(char*,std::size_t,void*);
    LocalUpdateServer();
    // Call request_stop/poll until stopped before destruction to keep UI responsive.
    // Unexpected active destruction cooperatively joins; it never frees live state.
    ~LocalUpdateServer();
    LocalUpdateServer(const LocalUpdateServer&)=delete;
    LocalUpdateServer& operator=(const LocalUpdateServer&)=delete;
    // Owner-thread lifecycle. Start alone initializes sockets; every launch is Off.
    // Report runs on the worker and must return promptly, at most kLocalUpdateReportCapacity bytes.
    bool start(const char* root,const char* app,const char* version,const char* token_file,ReportFn report,void* context);
    void request_stop();
    // Nonblocking reap: sockets remain owned until the worker actually exits.
    void poll();
    LocalUpdateSnapshot snapshot()const;
    bool stopped()const;
};
}
