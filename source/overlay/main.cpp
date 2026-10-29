#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <sys/stat.h>
#include "game_link.hpp"
#include "storage.hpp"
#include "re4/settings_file.hpp"

namespace {
GameLink link;
re4::Settings settings;
std::string note;
void record_connection() {
    if (!overlay_storage::ready()) return;
    mkdir("r4ovl:/switch", 0777); mkdir("r4ovl:/switch/re4-controls", 0777);
    FILE* file = std::fopen("r4ovl:/switch/re4-controls/game-inspection.txt", "wb");
    if (file) { std::fprintf(file, "%s\n", link.details.c_str()); std::fclose(file); }
}
void change() {
    const bool saved = overlay_storage::ready() && re4::save_settings_file(overlay_storage::settings_path, settings);
    link.apply(settings);
    note = saved ? link.message : "Не удалось сохранить настройки на SD";
}
class ControlsGui : public tsl::Gui {
    tsl::elm::ListItem* status_ = nullptr;
    tsl::elm::ToggleListItem* toggle_ = nullptr;
    tsl::elm::ListItem* speed_ = nullptr;
    tsl::elm::ListItem* deadzone_ = nullptr;
    unsigned frame_ = 0;
public:
    tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("RE4 Управление", "0.1.0 — экспериментальная версия");
        auto list = new tsl::elm::List;
        status_ = new tsl::elm::ListItem("Подключиться к игре", "A");
        status_->setClickListener([](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;
            link.connect(); record_connection(); note = link.message; return true;
        });
        list->addItem(status_);
        list->addItem(new tsl::elm::CategoryHeader("Повороты при ходьбе и беге"));
        toggle_ = new tsl::elm::ToggleListItem("Плавный поворот", settings.analog_enabled, "Вкл", "Выкл");
        toggle_->setStateChangedListener([](bool value) { settings.analog_enabled = value; change(); });
        list->addItem(toggle_);
        speed_ = add_value(list, "Скорость поворота", &settings.turn_gain, 0.1f, 0, 4);
        deadzone_ = add_value(list, "Мёртвая зона левого стика", &settings.left_deadzone, 0.01f, 0, 0.95f);
        auto reset = new tsl::elm::ListItem("Сбросить настройки", "A");
        reset->setClickListener([this](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;
            settings = {}; change(); toggle_->setState(settings.analog_enabled);
            speed_->setValue("1.00"); deadzone_->setValue("0.15"); return true;
        });
        list->addItem(reset);
        list->addItem(new tsl::elm::CategoryHeader("Ещё не включено в игровой модуль"));
        list->addItem(new tsl::elm::ListItem("Гироскоп и прицел", "Диагностика в NRO"));
        list->addItem(new tsl::elm::ListItem("Поворот на месте", "Не проверен"));
        frame->setContent(list);
        return frame;
    }
    tsl::elm::ListItem* add_value(tsl::elm::List* list, const char* label, float* value,
                   float step, float minimum, float maximum) {
        char text[24]; std::snprintf(text, sizeof(text), "%.2f", *value);
        auto item = new tsl::elm::ListItem(label, text);
        item->setClickListener([item, value, step, minimum, maximum](u64 keys) {
            if (!(keys & (HidNpadButton_Left | HidNpadButton_Right))) return false;
            *value = std::clamp(*value + ((keys & HidNpadButton_Right) ? step : -step), minimum, maximum);
            char text[24]; std::snprintf(text, sizeof(text), "%.2f", *value);
            item->setValue(text); change(); return true;
        });
        list->addItem(item);
        return item;
    }
    void update() override {
        if (++frame_ % 15 == 0 && status_) status_->setText(note.empty() ? link.message : note);
    }
};
class ControlsOverlay : public tsl::Overlay {
    void initServices() override {
        if (overlay_storage::mount()) {
            mkdir("r4ovl:/config", 0777); mkdir("r4ovl:/config/re4-controls", 0777);
            re4::load_settings_file(overlay_storage::settings_path, settings);
        } else note = "Не удалось подключить SD";
        link.initialize();
    }
    void exitServices() override { link.close(); overlay_storage::close(); }
    std::unique_ptr<tsl::Gui> loadInitialGui() override { return initially<ControlsGui>(); }
};
}
int main(int argc, char** argv) { return tsl::loop<ControlsOverlay>(argc, argv); }
