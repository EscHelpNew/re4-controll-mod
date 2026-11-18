#pragma once
#include "startup_options.hpp"
#include "settings.hpp"
#include "update.hpp"
#include <memory>

namespace re4 {
template<class Worker = UpdateWorker>
std::unique_ptr<Worker> create_startup_updater(const StartupOptions& options) {
    if (options.recovery) return nullptr;
    return std::make_unique<Worker>();
}

template<class Worker>
UpdateSnapshot startup_update_snapshot(const Worker* updater, Language language, const StartupOptions& options) {
    if (updater) return updater->snapshot();
    if (!options.recovery) return {};
    UpdateSnapshot disabled;
    disabled.stage = UpdateStage::Failed;
    disabled.detail = language == Language::Russian
        ? u8"Обновление по сети недоступно в режиме восстановления."
        : "Network updates are unavailable in recovery mode.";
    return disabled;
}
}
