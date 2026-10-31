#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "game_link.hpp"
#include "storage.hpp"
#include "re4/graphics_preview_image.hpp"
#include <array>
#include <sys/stat.h>

namespace {
Re6GameLink link;
re6::Settings settings;
std::string note;
const char* tr(const char* en, const char* ru) {
    return settings.language == re6::Language::Russian ? ru : en;
}
const char* mount_error() { return tr("Could not mount SD card", "Не удалось подключить SD"); }
bool change(const re6::Settings& candidate) {
    if (!re6_overlay_storage::ready()
        || !re6::save_settings_file(re6_overlay_storage::settings_path, candidate)) {
        note = tr("Could not save settings to SD", "Не удалось сохранить настройки на SD");
        return false;
    }
    settings = candidate;
    link.language = settings.language;
    link.apply(settings);
    note = link.message;
    return true;
}
class DescriptionFrame : public tsl::elm::OverlayFrame {
    std::string description_;
    unsigned help_y_ = tsl::cfg::FramebufferHeight - 152;
    bool graphics_preview_ = false;
    graphics_preview::Topic graphics_topic_ = graphics_preview::Topic::Filtering;
    int graphics_x_ = 64;
public:
    DescriptionFrame() : OverlayFrame(tr("RE6 Graphics", "Графика RE6"), "") {}
    void set_help(const char* value) { description_ = value; }
    void set_graphics_preview(bool enabled) { graphics_preview_ = enabled; }
    void set_graphics_topic(graphics_preview::Topic topic) { graphics_topic_ = topic; }
    void layout(u16 x, u16 y, u16 width, u16 height) override {
        OverlayFrame::layout(x, y, width, height);
        help_y_ = y + height - (graphics_preview_ ? 367 : 152);
        graphics_x_ = int(x) + (int(width) - 320) / 2;
        if (m_contentElement) {
            m_contentElement->setBoundaries(x + 35, y + 125, width - 85, height - 73 - 125 - (graphics_preview_ ? 325 : 110));
            m_contentElement->invalidate();
        }
    }
    void draw(tsl::gfx::Renderer* renderer) override {
        OverlayFrame::draw(renderer);
        renderer->drawRect(20, help_y_ - 21, tsl::cfg::FramebufferWidth - 40, 1, a(0xF93B));
        renderer->drawString(description_.c_str(), false, 20, help_y_, 16, a(tsl::style::color::ColorDescription));
        if (graphics_preview_) {
            const int image_y = int(help_y_) + 90;
            graphics_preview::draw_rows(graphics_preview::image(re4::GameTarget::RE6), graphics_x_, image_y,
                [&](int x, int y, int w, int h, const std::uint8_t* rgba) { renderer->drawBitmap(x, y, w, h, rgba); });
            const auto roi = graphics_preview::focus(re4::GameTarget::RE6, graphics_topic_);
            if (roi.width && roi.height) {
                const int x = graphics_x_ + int(roi.x), y = image_y + int(roi.y), w = int(roi.width), h = int(roi.height);
                renderer->drawRect(x, y, w, 2, a(0xF93B)); renderer->drawRect(x, y+h-2, w, 2, a(0xF93B));
                renderer->drawRect(x, y, 2, h, a(0xF93B)); renderer->drawRect(x+w-2, y, 2, h, a(0xF93B));
            }
            renderer->drawString(graphics_preview::caption(graphics_topic_, settings.language == re6::Language::Russian),
                false, 20, help_y_ + 282, 16, a(tsl::style::color::ColorDescription));
        }
    }
};
enum class GraphicsRow { Filter, AntiAliasing, Reset };
struct Spec { const char *en, *ru, *help_en, *help_ru; GraphicsRow id; };
const std::array<Spec, 3> graphics_specs{{
    {"Texture filtering", "Фильтрация текстур",
     "Native preserves the game's filtering.\nAF improves textures at an angle.\nChanges require a game restart.",
     "Штатная: исходная фильтрация игры.\nAF улучшает текстуры под углом.\nПосле изменения перезапустите игру.", GraphicsRow::Filter},
    {"Anti-aliasing", "Сглаживание",
     "AA applies live in the main renderer.\nNative preserves the game's setting.\nA / Left / Right: choose an AA mode.",
     "Сглаживание меняется в основном рендере.\nШтатное: исходное сглаживание игры.\nA / влево / вправо: выбрать режим.", GraphicsRow::AntiAliasing},
    {"Reset graphics", "Сброс графики",
     "Restore native filtering and AA.\nAA applies live; filtering needs a restart.\nThe selected language is kept. A: reset.",
     "Вернуть штатные фильтрацию и сглаживание.\nФильтрация: после перезапуска игры.\nЯзык сохранится. A: сбросить.", GraphicsRow::Reset}
}};
std::string filter_text() {
    constexpr const char* values[] = {"Native", "AF 2x", "AF 4x", "AF 8x", "AF 16x"};
    return settings.texture_filtering == re6::TextureFilter::Native ? tr("Native", "Штатная")
        : values[unsigned(settings.texture_filtering)];
}
std::string aa_text() {
    constexpr const char* values[] = {"Native", "Off", "FXAA", "FXAA3", "FXAA3HQ"};
    if (settings.anti_aliasing == re6::AntiAliasing::Native) return tr("Native", "Штатное");
    if (settings.anti_aliasing == re6::AntiAliasing::Off) return tr("Off", "Выкл.");
    return values[unsigned(settings.anti_aliasing)];
}
class GraphicsGui : public tsl::Gui {
    DescriptionFrame* frame_ = nullptr;
    std::array<tsl::elm::ListItem*, 3> rows_{};
public:
    tsl::elm::Element* createUI() override {
        frame_ = new DescriptionFrame;
        frame_->set_graphics_preview(true);
        auto* list = new tsl::elm::List;
        for (unsigned row = 0; row < rows_.size(); ++row) {
            const auto& entry = graphics_specs[row];
            auto* item = new tsl::elm::ListItem(tr(entry.en, entry.ru));
            item->setClickListener([this, row](u64 keys) {
                const auto id = graphics_specs[row].id;
                const bool reset = id == GraphicsRow::Reset;
                if (reset ? !(keys & HidNpadButton_A)
                    : !(keys & (HidNpadButton_A | HidNpadButton_Left | HidNpadButton_Right))) return false;
                auto candidate = settings;
                const int direction = (keys & HidNpadButton_Left) ? -1 : 1;
                if (reset) {
                    candidate.texture_filtering = re6::TextureFilter::Native;
                    candidate.anti_aliasing = re6::AntiAliasing::Native;
                } else if (id == GraphicsRow::AntiAliasing) candidate.anti_aliasing = static_cast<re6::AntiAliasing>(
                    (int(settings.anti_aliasing) + direction + 5) % 5);
                else candidate.texture_filtering = static_cast<re6::TextureFilter>(
                    (int(settings.texture_filtering) + direction + 5) % 5);
                if (candidate.texture_filtering != settings.texture_filtering || candidate.anti_aliasing != settings.anti_aliasing) change(candidate);
                update(); return true;
            });
            rows_[row] = item; list->addItem(item);
        }
        frame_->setContent(list); update(); return frame_;
    }
    void update() override {
        frame_->setTitle(tr("RE6 Graphics", "Графика RE6"));
        frame_->setSubtitle(note.empty() ? tr("Graphics", "Графика") : note);
        unsigned focused = 0;
        for (unsigned row = 0; row < rows_.size(); ++row) {
            const auto& entry = graphics_specs[row];
            rows_[row]->setText(tr(entry.en, entry.ru));
            rows_[row]->setValue(entry.id == GraphicsRow::Reset ? "A"
                : entry.id == GraphicsRow::AntiAliasing ? aa_text() : filter_text());
            if (getFocusedElement() == rows_[row]) focused = row;
        }
        frame_->set_help(tr(graphics_specs[focused].help_en, graphics_specs[focused].help_ru));
        frame_->set_graphics_topic(graphics_preview::topic_for(re4::GameTarget::RE6, focused));
    }
};
class MainGui : public tsl::Gui {
    DescriptionFrame* frame_ = nullptr;
    tsl::elm::ListItem *graphics_ = nullptr, *connect_ = nullptr, *language_ = nullptr;
public:
    tsl::elm::Element* createUI() override {
        frame_ = new DescriptionFrame;
        auto* list = new tsl::elm::List;
        graphics_ = new tsl::elm::ListItem("Graphics", "A");
        graphics_->setClickListener([](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;
            note.clear(); tsl::changeTo<GraphicsGui>(); return true;
        });
        connect_ = new tsl::elm::ListItem("Connect to RE6", "A");
        connect_->setClickListener([this](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;
            if (!re6_overlay_storage::ready()) note = mount_error();
            else {
                link.language = settings.language;
                if (link.connect()) link.apply(settings);
                note = link.message;
            }
            update(); return true;
        });
        language_ = new tsl::elm::ListItem("Language");
        language_->setClickListener([this](u64 keys) {
            if (!(keys & (HidNpadButton_A | HidNpadButton_Left | HidNpadButton_Right))) return false;
            auto candidate = settings;
            candidate.language = settings.language == re6::Language::English ? re6::Language::Russian : re6::Language::English;
            change(candidate); update(); return true;
        });
        for (auto* row : {graphics_, connect_, language_}) list->addItem(row);
        frame_->setContent(list); update(); return frame_;
    }
    void update() override {
        frame_->setTitle(tr("RE6 Graphics", "Графика RE6"));
        frame_->setSubtitle(note.empty() ? tr("Filtering and anti-aliasing", "Фильтрация и сглаживание") : note);
        graphics_->setText(tr("Graphics", "Графика"));
        connect_->setText(tr("Connect to RE6", "Подключиться к RE6"));
        language_->setText(tr("Language", "Язык"));
        language_->setValue(settings.language == re6::Language::Russian ? "Русский" : "English");
        if (getFocusedElement() == connect_) frame_->set_help(tr(
            "Start RE6 with the graphics module.\nA: connect and send the saved settings.",
            "Запустите RE6 с модулем графики.\nA: подключиться и передать настройки."));
        else if (getFocusedElement() == language_) frame_->set_help(tr(
            "Choose English or Russian for this overlay.\nA / Left / Right: switch language.",
            "English / Русский: язык этого оверлея.\nA / влево / вправо: сменить язык."));
        else frame_->set_help(tr(
            "Filtering changes require a game restart.\nAA applies live in the main renderer.\nA: open. B: return.",
            "Фильтрация: после перезапуска игры.\nСглаживание: в основном рендере игры.\nA: открыть. B: назад."));
    }
};
class GraphicsOverlay : public tsl::Overlay {
public:
    void initServices() override {
        settings = {}; note.clear();
        const bool mounted = re6_overlay_storage::mount();
        if (mounted) {
            mkdir("r6ovl:/config", 0777); mkdir("r6ovl:/config/re4-controls", 0777);
            re6::Settings loaded;
            if (re6::load_settings_file(re6_overlay_storage::settings_path, loaded)) settings = loaded;
        }
        link.language = settings.language; link.initialize();
        if (mounted) {
            if (link.connect()) link.apply(settings);
            note = link.message;
        } else note = mount_error();
    }
    void exitServices() override { link.close(); re6_overlay_storage::close(); }
    std::unique_ptr<tsl::Gui> loadInitialGui() override { return initially<MainGui>(); }
};
}
int main(int argc, char** argv) { return tsl::loop<GraphicsOverlay>(argc, argv); }
