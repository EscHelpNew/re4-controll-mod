#pragma once
#include <switch.h>
#include "re4/settings.hpp"
#include "re4/shared.hpp"
#include <string>

class GameLink {
    Service service_{};
    u64 process_ = 0, address_ = 0;
    bool info_ready_ = false;
    const char* tr(const char* english,const char* russian) const {
        return language==re4::Language::Russian?russian:english;
    }
public:
    re4::Language language=re4::Language::English;
    std::string details;
    std::string message = "Not connected";
    void initialize();
    void close();
    bool connect();
    bool apply(const re4::Settings& settings);
};
