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
class DescriptionFrame : public tsl::elm::OverlayFrame {
    const char* description_ = "Выберите настройку: здесь появится\nеё описание.";
    unsigned help_y_ = tsl::cfg::FramebufferHeight - 152;
public:
    DescriptionFrame() : OverlayFrame("RE4 Управление", "0.2.3 — настройки управления") {}
    void set_help(const char* text) { description_ = text; }
    void layout(u16 x, u16 y, u16 width, u16 height) override {
        OverlayFrame::layout(x, y, width, height);
        help_y_ = y + height - 152;
        if (m_contentElement) {
            m_contentElement->setBoundaries(x + 35, y + 125, width - 85, height - 73 - 125 - 110);
            m_contentElement->invalidate();
        }
    }
    void draw(tsl::gfx::Renderer* renderer) override {
        OverlayFrame::draw(renderer);
        renderer->drawRect(20, help_y_ - 21, tsl::cfg::FramebufferWidth - 40, 1, a(tsl::style::color::ColorDescription));
        renderer->drawString(description_, false, 20, help_y_, 16, a(tsl::style::color::ColorDescription));
    }
};
class ControlsGui : public tsl::Gui {
    tsl::elm::ListItem* status_ = nullptr;
    tsl::elm::ToggleListItem* toggle_ = nullptr;
    tsl::elm::ListItem* speed_ = nullptr;
    tsl::elm::ListItem* deadzone_ = nullptr;
    std::vector<std::pair<tsl::elm::ListItem*, float*>> values_;
    tsl::elm::ToggleListItem* gyro_ = nullptr;
    tsl::elm::ToggleListItem* invert_x_ = nullptr;
    tsl::elm::ToggleListItem* invert_y_ = nullptr;
    DescriptionFrame* description_frame_ = nullptr;
    std::vector<std::pair<tsl::elm::Element*, const char*>> help_;
    unsigned frame_ = 0;
public:
    tsl::elm::Element* createUI() override {
        values_.clear();
        help_.clear();
        auto frame = description_frame_ = new DescriptionFrame;
        auto list = new tsl::elm::List;
        status_ = new tsl::elm::ListItem("Подключиться к игре", "A");
        status_->setClickListener([](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;
            link.connect(); record_connection(); note = link.message; return true;
        });
        list->addItem(status_);
        help_.push_back({status_, "A: найти мод в запущенной RE4.\nПосле перезапуска игры подключитесь снова.\nИзменения применяются без выхода из игры."});
        list->addItem(new tsl::elm::CategoryHeader("Стики и движение"));
        toggle_ = new tsl::elm::ToggleListItem("Плавное управление", settings.analog_enabled, "Вкл", "Выкл");
        toggle_->setStateChangedListener([](bool value) { settings.analog_enabled = value; change(); });
        list->addItem(toggle_);
        help_.push_back({toggle_, "Плавные повороты, ходьба, камера и прицел.\nЧем меньше отклонение стика, тем медленнее\nдвижение. Гироскоп включается отдельно."});
        speed_ = add_value(list, "Скорость поворота", &settings.turn_gain, 0.1f, 0, 1,
            "Поворот персонажа левым стиком.\n0: без поворота; 1: максимум штатной игры.\nУскорение выше штатного недоступно.");
        add_value(list, "Скорость ходьбы и бега", &settings.movement_gain, 0.1f, 0, 1,
            "Темп движения вперёд и назад.\nНебольшое отклонение даёт медленную ходьбу.\n1: штатный максимум; 0: движение остановлено.");
        deadzone_ = add_value(list, "Мёртвая зона левого стика", &settings.left_deadzone, 0.01f, 0, 0.95f,
            "Область около центра без реакции.\nУвеличьте при дрейфе левого стика.\nМеньше значение — раньше начинается движение.");
        add_value(list, "Чувствительность камеры", &settings.camera_gain, 0.1f, 0, 1,
            "Обзор правым стиком вне прицеливания.\nМеньше значение — мягче отклик камеры.\n1: полный штатный диапазон обзора.");
        add_value(list, "Мёртвая зона правого стика", &settings.right_deadzone, 0.01f, 0, 0.95f,
            "Область около центра правого стика.\nУвеличьте, если камера двигается сама.\nГироскоп этой настройкой не затрагивается.");
        list->addItem(new tsl::elm::CategoryHeader("Прицел"));
        add_value(list, "Прицел по горизонтали", &settings.aim_gain_x, 0.1f, 0, 1,
            "Скорость прицела влево и вправо от стика.\n0: без движения; 1: штатный максимум.\nЧувствительность гироскопа задаётся отдельно.");
        add_value(list, "Прицел по вертикали", &settings.aim_gain_y, 0.1f, 0, 1,
            "Скорость прицела вверх и вниз от стика.\n0: без движения; 1: штатный максимум.\nНе ограничивает вертикальный гироскоп.");
        list->addItem(new tsl::elm::CategoryHeader("Гироскоп — только прицеливание"));
        gyro_ = new tsl::elm::ToggleListItem("Прицеливание гироскопом", settings.gyro_enabled, "Вкл", "Выкл");
        gyro_->setStateChangedListener([](bool value) { settings.gyro_enabled = value; change(); });
        list->addItem(gyro_);
        help_.push_back({gyro_, "Наклон дополняет обычное управление стиком.\nДействует только при наведении из оружия.\nПри ходьбе и после отпускания прицела выключен."});
        add_value(list, "Гироскоп по горизонтали", &settings.gyro_gain_x, 0.1f, 0, 10,
            "Отклик прицела на поворот контроллера.\n0: ось выключена; больше — чувствительнее.\nНе меняет скорость ходьбы и стиков.");
        add_value(list, "Гироскоп по вертикали", &settings.gyro_gain_y, 0.1f, 0, 10,
            "Отклик прицела на наклон вверх и вниз.\nНастраивается независимо от горизонтали.\nДоступен штатный вертикальный диапазон.");
        invert_x_ = new tsl::elm::ToggleListItem("Инверсия гироскопа X", settings.invert_x, "Вкл", "Выкл");
        invert_x_->setStateChangedListener([](bool value) { settings.invert_x = value; change(); });
        list->addItem(invert_x_);
        help_.push_back({invert_x_, "Меняет направление горизонтального гироскопа.\nНе меняет направление движения стика.\nПо умолчанию включено."});
        invert_y_ = new tsl::elm::ToggleListItem("Инверсия гироскопа Y", settings.invert_y, "Вкл", "Выкл");
        invert_y_->setStateChangedListener([](bool value) { settings.invert_y = value; change(); });
        list->addItem(invert_y_);
        help_.push_back({invert_y_, "Меняет направление вертикального гироскопа.\nНе меняет направление движения стика.\nПо умолчанию включено."});
        auto reset = new tsl::elm::ListItem("Сбросить настройки", "A");
        reset->setClickListener([this](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;
            settings = {}; change(); toggle_->setState(settings.analog_enabled);
            gyro_->setState(settings.gyro_enabled);
            invert_x_->setState(settings.invert_x); invert_y_->setState(settings.invert_y);
            for (const auto& entry : values_) {
                char text[24]; std::snprintf(text, sizeof(text), "%.2f", *entry.second);
                entry.first->setValue(text);
            }
            return true;
        });
        list->addItem(reset);
        help_.push_back({reset, "A: вернуть значения по умолчанию и сохранить.\nСтики: 1; мёртвые зоны: 0.15.\nГироскоп выключен; инверсия X/Y включена."});
        frame->setContent(list);
        return frame;
    }
    tsl::elm::ListItem* add_value(tsl::elm::List* list, const char* label, float* value,
                   float step, float minimum, float maximum, const char* description) {
        char text[24]; std::snprintf(text, sizeof(text), "%.2f", *value);
        auto item = new tsl::elm::ListItem(label, text);
        item->setClickListener([item, value, step, minimum, maximum](u64 keys) {
            if (!(keys & (HidNpadButton_Left | HidNpadButton_Right))) return false;
            *value = std::clamp(*value + ((keys & HidNpadButton_Right) ? step : -step), minimum, maximum);
            char text[24]; std::snprintf(text, sizeof(text), "%.2f", *value);
            item->setValue(text); change(); return true;
        });
        list->addItem(item);
        values_.push_back({item, value});
        help_.push_back({item, description});
        return item;
    }
    void update() override {
        if (description_frame_) for (const auto& entry : help_)
            if (entry.first == getFocusedElement()) { description_frame_->set_help(entry.second); break; }
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
