#pragma once
#include <switch.h>
#include "re6/settings.hpp"
#include "re6/shared.hpp"
#include <string>

class Re6GameLink {
    Service service_{};
    u64 process_=0,address_=0;
    bool info_ready_=false;
    const char* tr(const char* en,const char* ru)const{return language==re6::Language::Russian?ru:en;}
public:
    re6::Language language=re6::Language::English;
    std::string details,message="Not connected";
    re6::SharedControl diagnostics{};
    void initialize();
    void close();
    bool connect();
    bool apply(const re6::Settings& settings);
};
