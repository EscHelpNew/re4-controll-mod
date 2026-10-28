#pragma once
#include <switch.h>
#include "re4/settings.hpp"
#include "re4/shared.hpp"
#include <string>

class GameLink {
    Service service_{};
    u64 process_ = 0, address_ = 0;
    bool info_ready_ = false;
public:
    std::string details;
    std::string message = "Подключение не выполнено";
    void initialize();
    void close();
    bool connect();
    bool apply(const re4::Settings& settings);
};
