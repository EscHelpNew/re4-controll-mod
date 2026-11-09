#pragma once
#include <switch.h>
#include "re4/settings.hpp"
#include "re4/shared.hpp"
#include "re4/live_diagnostics.hpp"
#include "re4/money_bonus.hpp"
#include <array>
#include <string>

class GameLink {
    Service service_{};
    u64 process_ = 0, address_ = 0;
    bool info_ready_ = false;
    bool borrowed_ = false; // Session owned by another GameLink; never closed here.
    const char* tr(const char* english,const char* russian) const {
        return language==re4::Language::Russian?russian:english;
    }
public:
    re4::Language language=re4::Language::English;
    std::string details;
    std::string message = "Not connected";
    re4::CharacterAppearanceSnapshot appearance;
    re4::MoneySnapshot money;
    std::uint32_t popup_diagnostic=0;
    bool popup_bounds_valid=false;
    std::array<float,4> popup_viewport{};
    std::array<std::int32_t,4> popup_scissor{};
    void initialize();
    // Reuse owner's dmnt:cht session. Opening a second one can block forever in
    // sm when the service's session limit is reached. Owner must outlive this.
    void initialize_shared(const GameLink& owner);
    void close();
    bool connect();
    bool apply(const re4::Settings& settings);
    bool refresh_appearance();
    bool refresh_popup();
    bool refresh_money();
    bool request_money();
    // Use a separate worker-owned instance. False still fills an error/status
    // snapshot which may be formatted and returned to a diagnostic client.
    bool read_diagnostic_snapshot(re4::LiveDiagnosticSnapshot&);
};
