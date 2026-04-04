#include "item_tooltip_ui.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_element_helpers.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace {

using engine::ui::rmlui::setPixelProperty;
using engine::ui::rmlui::snapToPixel;
using engine::ui::rmlui::textToInnerRml;

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/item_tooltip.rml";

} // namespace

namespace game::ui {

ItemTooltipUI::ItemTooltipUI(engine::core::Context& context, uint64_t owner_scene_id)
    : context_(context) {
    initDocument(owner_scene_id);
    hideTooltip();
}

ItemTooltipUI::~ItemTooltipUI() {
    if (document_ && runtime_) {
        runtime_->unloadDocument(document_);
    }
    document_ = nullptr;
    panel_ = nullptr;
    name_element_ = nullptr;
    category_element_ = nullptr;
    description_element_ = nullptr;
}

void ItemTooltipUI::initDocument(uint64_t owner_scene_id) {
    runtime_ = context_.getRmlUi();
    if (!runtime_) {
        spdlog::error("ItemTooltipUI: RmlUiRuntime 不可用。");
        return;
    }

    document_ = runtime_->loadDocument(DOCUMENT_PATH, owner_scene_id);
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
        runtime_->unloadDocument(document_);
        document_ = nullptr;
        panel_ = nullptr;
        name_element_ = nullptr;
        category_element_ = nullptr;
        description_element_ = nullptr;
        return;
    }

    runtime_->hideDocument(document_);
}

void ItemTooltipUI::refreshLayoutMetrics() {
    if (!document_ || !panel_) {
        return;
    }

    document_->UpdateDocument();
    size_ = {
        snapToPixel(panel_->GetOffsetWidth()),
        snapToPixel(panel_->GetOffsetHeight()),
    };
}

void ItemTooltipUI::showItem(std::string_view display_name,
                             std::string_view category,
                             std::string_view description) {
    if (!document_ || !runtime_ || !panel_) {
        return;
    }

    display_name_ = std::string(display_name);
    category_ = std::string(category);
    description_ = std::string(description);

    name_element_->SetInnerRML(textToInnerRml(display_name_));
    category_element_->SetInnerRML(textToInnerRml(category_));
    description_element_->SetInnerRML(textToInnerRml(description_));

    visible_ = true;
    runtime_->showDocument(document_);
    refreshLayoutMetrics();
}

void ItemTooltipUI::hideTooltip() {
    visible_ = false;
    if (document_ && runtime_) {
        runtime_->hideDocument(document_);
    }
}

void ItemTooltipUI::update(float delta_time) {
    (void)delta_time;
    if (!visible_ || !panel_) {
        return;
    }

    const glm::vec2 mouse_pos = context_.getInputManager().getLogicalMousePosition();
    const glm::vec2 logical_size = context_.getGameState().getLogicalSize();

    glm::vec2 pos = mouse_pos + offset_;
    if (pos.x + size_.x > logical_size.x) {
        pos.x = mouse_pos.x - offset_.x - size_.x;
    }
    if (pos.y + size_.y > logical_size.y) {
        pos.y = mouse_pos.y - offset_.y - size_.y;
    }

    pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, logical_size.x - size_.x));
    pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, logical_size.y - size_.y));

    setPixelProperty(panel_, "left", pos.x);
    setPixelProperty(panel_, "top", pos.y);
}

} // namespace game::ui
