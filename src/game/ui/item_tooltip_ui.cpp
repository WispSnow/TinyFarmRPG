#include "item_tooltip_ui.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/render/text_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "engine/ui/rmlui/rml_element_helpers.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace {

using engine::ui::rmlui::setFontSizeProperty;
using engine::ui::rmlui::setPaddingProperties;
using engine::ui::rmlui::setPixelProperty;
using engine::ui::rmlui::snapToPixel;
using engine::ui::rmlui::textToInnerRml;

constexpr float MIN_TOOLTIP_WIDTH = 120.0f;
constexpr float MIN_TOOLTIP_HEIGHT = 24.0f;
constexpr int CATEGORY_FONT_SIZE = 14;
constexpr int DESCRIPTION_FONT_SIZE = 16;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/item_tooltip.rml";

[[nodiscard]] std::size_t utf8Next(std::string_view text, std::size_t index) {
    if (index >= text.size()) {
        return text.size();
    }
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
                             uint64_t owner_scene_id,
                             entt::id_type font_id,
                             int font_size)
    : UIElement(glm::vec2{0.0f, 0.0f}),
      context_(context),
      font_id_(engine::ui::resolveUIFontId(font_id)),
      font_size_(font_size) {
    setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    setPivot({0.0f, 0.0f});
    setSize(glm::vec2{MIN_TOOLTIP_WIDTH, MIN_TOOLTIP_HEIGHT});
    initDocument(owner_scene_id);
    hideTooltip();
}

ItemTooltipUI::~ItemTooltipUI() {
    if (document_ && layer_) {
        layer_->unloadDocument(document_);
    }
    document_ = nullptr;
    panel_ = nullptr;
    name_element_ = nullptr;
    category_element_ = nullptr;
    description_element_ = nullptr;
}

void ItemTooltipUI::initDocument(uint64_t owner_scene_id) {
    layer_ = context_.getGLRenderer().getRmlUILayer();
    if (!layer_) {
        spdlog::error("ItemTooltipUI: RmlUILayer 不可用。");
        return;
    }

    document_ = layer_->loadDocument(DOCUMENT_PATH, owner_scene_id);
    if (!document_) {
        spdlog::error("ItemTooltipUI: 加载 RML 文档失败: {}", DOCUMENT_PATH);
        return;
    }

    panel_ = document_->GetElementById("item-tooltip-panel");
    name_element_ = document_->GetElementById("item-tooltip-name");
    category_element_ = document_->GetElementById("item-tooltip-category");
    description_element_ = document_->GetElementById("item-tooltip-description");
    if (!panel_ || !name_element_ || !category_element_ || !description_element_) {
        spdlog::error("ItemTooltipUI: RML 元素缺失。");
        if (layer_) {
            layer_->unloadDocument(document_);
        }
        document_ = nullptr;
        panel_ = nullptr;
        name_element_ = nullptr;
        category_element_ = nullptr;
        description_element_ = nullptr;
        return;
    }

    setPaddingProperties(panel_, padding_);
    setFontSizeProperty(name_element_, static_cast<float>(font_size_));
    setFontSizeProperty(category_element_, static_cast<float>(CATEGORY_FONT_SIZE));
    setFontSizeProperty(description_element_, static_cast<float>(DESCRIPTION_FONT_SIZE));
    layer_->hideDocument(document_);
}

void ItemTooltipUI::setPadding(const engine::ui::Thickness& padding) {
    padding_ = padding;
    if (panel_) {
        setPaddingProperties(panel_, padding_);
    }
    refreshLayout();
}

std::string ItemTooltipUI::wrapText(std::string_view text, int font_size) const {
    if (text.empty()) {
        return {};
    }

    auto& text_renderer = context_.getTextRenderer();

    std::string out;
    out.reserve(text.size() + 8);

    std::string line;
    line.reserve(text.size());
    std::size_t last_break_pos = std::string::npos;

    const auto fits = [&](const std::string& s) {
        return text_renderer.getTextSize(s, font_id_, font_size).x <= max_text_width_;
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

void ItemTooltipUI::refreshLayout() {
    if (!panel_ || !name_element_ || !category_element_ || !description_element_) {
        return;
    }

    const std::string wrapped_name = wrapText(display_name_, font_size_);
    const std::string wrapped_category = wrapText(category_, CATEGORY_FONT_SIZE);
    const std::string wrapped_description = wrapText(description_, DESCRIPTION_FONT_SIZE);

    name_element_->SetInnerRML(textToInnerRml(wrapped_name));
    category_element_->SetInnerRML(textToInnerRml(wrapped_category));
    description_element_->SetInnerRML(textToInnerRml(wrapped_description));

    auto& text_renderer = context_.getTextRenderer();
    const glm::vec2 name_size = text_renderer.getTextSize(wrapped_name, font_id_, font_size_);
    const glm::vec2 category_size = text_renderer.getTextSize(wrapped_category, font_id_, CATEGORY_FONT_SIZE);
    const glm::vec2 description_size = text_renderer.getTextSize(wrapped_description, font_id_, DESCRIPTION_FONT_SIZE);

    const float content_width = std::max({name_size.x, category_size.x, description_size.x});
    const float content_height = name_size.y + spacing_ + category_size.y + spacing_ + description_size.y;

    const glm::vec2 outer_size{
        snapToPixel(std::max(MIN_TOOLTIP_WIDTH, content_width + padding_.width())),
        snapToPixel(std::max(MIN_TOOLTIP_HEIGHT, content_height + padding_.height()))
    };
    const glm::vec2 content_box_size{
        snapToPixel(std::max(0.0f, outer_size.x - padding_.width())),
        snapToPixel(std::max(0.0f, outer_size.y - padding_.height()))
    };

    setSize(outer_size);
    setPixelProperty(panel_, "width", content_box_size.x);
    setPixelProperty(panel_, "height", content_box_size.y);
}

void ItemTooltipUI::showItem(std::string_view display_name,
                             std::string_view category,
                             std::string_view description) {
    if (!document_) {
        return;
    }

    display_name_ = std::string(display_name);
    category_ = std::string(category);
    description_ = std::string(description);
    refreshLayout();
    UIElement::setVisible(true);
    if (layer_) {
        layer_->showDocument(document_);
    }
}

void ItemTooltipUI::hideTooltip() {
    UIElement::setVisible(false);
    if (document_ && layer_) {
        layer_->hideDocument(document_);
    }
}

void ItemTooltipUI::update(float delta_time, engine::core::Context& context) {
    UIElement::update(delta_time, context);

    if (!isVisible() || !panel_) {
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
    pos.x = snapToPixel(pos.x);
    pos.y = snapToPixel(pos.y);

    setPosition(pos);
    setPixelProperty(panel_, "left", pos.x);
    setPixelProperty(panel_, "top", pos.y);
}

} // namespace game::ui
