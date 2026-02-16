#include "item_tooltip_ui.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/ui_preset_manager.h"
#include <algorithm>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace {

constexpr entt::id_type HOVER_PANEL_PRESET_ID = "hover_panel"_hs;
constexpr float MIN_TOOLTIP_WIDTH = 120.0f;
constexpr float MIN_TOOLTIP_HEIGHT = 24.0f;

[[nodiscard]] std::size_t utf8Next(std::string_view text, std::size_t index) {
    if (index >= text.size()) return text.size();
    const unsigned char c = static_cast<unsigned char>(text[index]);
    if ((c & 0x80u) == 0u) return index + 1;
    if ((c & 0xE0u) == 0xC0u) return std::min(text.size(), index + 2);
    if ((c & 0xF0u) == 0xE0u) return std::min(text.size(), index + 3);
    if ((c & 0xF8u) == 0xF0u) return std::min(text.size(), index + 4);
    return index + 1;
}

[[nodiscard]] bool isBreakableSpace(std::string_view chunk) {
    return chunk == " " || chunk == "\t";
}

void trimLeftSpaces(std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    if (i > 0) {
        s.erase(0, i);
    }
}

void trimRightSpaces(std::string& s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
}

} // namespace

namespace game::ui {

ItemTooltipUI::ItemTooltipUI(engine::core::Context& context,
                             std::string_view font_path,
                             int font_size)
    : UIElement(glm::vec2{0.0f, 0.0f}),
      context_(context),
      font_path_(font_path),
      font_size_(font_size) {
    setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    setPivot({0.0f, 0.0f});

    buildSkin();
    buildLayout();
    hideTooltip();
}

void ItemTooltipUI::setPadding(const engine::ui::Thickness& padding) {
    padding_ = padding;
    if (panel_) {
        panel_->setPadding(padding_);
    }
    refreshLayout();
}

void ItemTooltipUI::buildSkin() {
    // 皮肤由 buildLayout() 创建 UIPanel 时读取
}

void ItemTooltipUI::buildLayout() {
    auto& preset_mgr = context_.getResourceManager().getUIPresetManager();
    auto& text_renderer = context_.getTextRenderer();
    engine::render::Image panel_image{};
    if (const auto* preset = preset_mgr.getImagePreset(HOVER_PANEL_PRESET_ID)) {
        panel_image = *preset;
    }

    auto panel = std::make_unique<engine::ui::UIPanel>(glm::vec2{0.0f, 0.0f},
                                                       glm::vec2{MIN_TOOLTIP_WIDTH, MIN_TOOLTIP_HEIGHT},
                                                       std::nullopt,
                                                       panel_image);
    panel->setPivot({0.0f, 0.0f});
    panel->setPadding(padding_);
    panel_ = panel.get();

    auto name_label =
        std::make_unique<engine::ui::UILabel>(text_renderer, "", font_path_, font_size_, glm::vec2{0.0f, 0.0f}, engine::utils::FColor::black());
    name_label_ = name_label.get();
    name_label_->setPivot({0.0f, 0.0f});
    name_label_->setShadowColor(engine::utils::FColor::white());

    auto category_label = std::make_unique<engine::ui::UILabel>(
        text_renderer, "", font_path_, 14, glm::vec2{0.0f, 0.0f}, engine::utils::FColor{0.2, 0.2, 0.2, 1.0f});
    category_label_ = category_label.get();
    category_label_->setPivot({0.0f, 0.0f});
    category_label_->setShadowEnabled(false);

    auto description_label =
        std::make_unique<engine::ui::UILabel>(text_renderer, "", font_path_, 16, glm::vec2{0.0f, 0.0f}, engine::utils::FColor::black());
    description_label_ = description_label.get();
    description_label_->setPivot({0.0f, 0.0f});
    description_label_->setShadowEnabled(false);

    panel_->addChild(std::move(name_label));
    panel_->addChild(std::move(category_label));
    panel_->addChild(std::move(description_label));
    addChild(std::move(panel));
}

std::string ItemTooltipUI::wrapText(std::string_view text) const {
    if (text.empty()) return {};

    auto& text_renderer = context_.getTextRenderer();
    const entt::id_type font_id = entt::hashed_string{font_path_.c_str(), font_path_.size()};

    std::string out;
    out.reserve(text.size() + 8);

    std::string line;
    line.reserve(text.size());
    std::size_t last_break_pos = std::string::npos; // 在 line 中可断行的位置（通常是空格后）

    const auto fits = [&](const std::string& s) {
        return text_renderer.getTextSize(s, font_id, font_size_, font_path_).x <= max_text_width_;
    };

    std::size_t i = 0;
    while (i < text.size()) {
        const char ch = text[i];
        if (ch == '\r') {
            ++i;
            continue;
        }
        if (ch == '\n') {
            trimRightSpaces(line);
            out.append(line);
            out.push_back('\n');
            line.clear();
            last_break_pos = std::string::npos;
            ++i;
            continue;
        }

        const std::size_t next = utf8Next(text, i);
        std::string_view chunk = text.substr(i, next - i);

        std::string candidate = line;
        candidate.append(chunk);

        if (line.empty() || fits(candidate)) {
            line = std::move(candidate);
            if (isBreakableSpace(chunk)) {
                last_break_pos = line.size();
            }
            i = next;
            continue;
        }

        if (last_break_pos != std::string::npos) {
            std::string remainder = line.substr(last_break_pos);
            line.erase(last_break_pos);
            trimRightSpaces(line);
            trimLeftSpaces(remainder);

            out.append(line);
            out.push_back('\n');
            line = std::move(remainder);
            last_break_pos = std::string::npos;
            continue;
        }

        trimRightSpaces(line);
        out.append(line);
        out.push_back('\n');
        line.clear();
        last_break_pos = std::string::npos;
    }

    trimRightSpaces(line);
    out.append(line);
    return out;
}

void ItemTooltipUI::showItem(std::string_view display_name,
                             std::string_view category,
                             std::string_view description) {
    if (!panel_ || !name_label_ || !category_label_ || !description_label_) {
        return;
    }

    name_label_->setText(wrapText(display_name));
    category_label_->setText(wrapText(category));
    description_label_->setText(wrapText(description));

    refreshLayout();
    setVisible(true);
}

void ItemTooltipUI::hideTooltip() {
    setVisible(false);
}

void ItemTooltipUI::refreshLayout() {
    if (!panel_ || !name_label_ || !category_label_ || !description_label_) {
        return;
    }

    const glm::vec2 name_size = name_label_->getRequestedSize();
    const glm::vec2 category_size = category_label_->getRequestedSize();
    const glm::vec2 desc_size = description_label_->getRequestedSize();

    const float content_width = std::max({name_size.x, category_size.x, desc_size.x});
    const float content_height = name_size.y + spacing_ + category_size.y + spacing_ + desc_size.y;

    glm::vec2 panel_size{
        std::max(MIN_TOOLTIP_WIDTH, content_width + padding_.width()),
        std::max(MIN_TOOLTIP_HEIGHT, content_height + padding_.height())
    };

    setSize(panel_size);
    panel_->setSize(panel_size);

    float y = 0.0f;
    name_label_->setPosition(glm::vec2{0.0f, y});
    y += name_size.y + spacing_;
    category_label_->setPosition(glm::vec2{0.0f, y});
    y += category_size.y + spacing_;
    description_label_->setPosition(glm::vec2{0.0f, y});
}

void ItemTooltipUI::update(float delta_time, engine::core::Context& context) {
    UIElement::update(delta_time, context);

    if (!isVisible()) {
        return;
    }

    const glm::vec2 mouse_pos = context_.getInputManager().getLogicalMousePosition();
    const glm::vec2 logical_size = context_.getGameState().getLogicalSize();
    const glm::vec2 tooltip_size = getRequestedSize();

    glm::vec2 pos = mouse_pos + offset_;

    if (pos.x + tooltip_size.x > logical_size.x) {
        pos.x = mouse_pos.x - offset_.x - tooltip_size.x;
    }
    if (pos.y + tooltip_size.y > logical_size.y) {
        pos.y = mouse_pos.y - offset_.y - tooltip_size.y;
    }

    pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, logical_size.x - tooltip_size.x));
    pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, logical_size.y - tooltip_size.y));

    setPosition(pos);
}

} // namespace game::ui
