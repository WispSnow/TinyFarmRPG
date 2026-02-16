#include "hotbar_ui.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/resource/resource_manager.h"
#include "engine/input/input_manager.h"
#include "engine/ui/ui_panel.h"
#include "engine/ui/ui_preset_manager.h"
#include "engine/ui/behavior/drag_behavior.h"
#include "engine/ui/behavior/hover_behavior.h"
#include "engine/ui/ui_manager.h"
#include "game/ui/inventory_ui.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/ui_drag_drop_helpers.h"
#include "game/defs/events.h"
#include "game/component/hotbar_component.h"
#include <algorithm>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace {
constexpr entt::hashed_string QUICK_BAR_PANEL_PRESET{"quick_bar_panel"};
constexpr entt::hashed_string QUICK_BAR_SLOT_PRESET{"quick_bar_slot"};
constexpr entt::hashed_string QUICK_BAR_SELECTED_PRESET{"quick_bar_selected"};
constexpr float BOTTOM_MARGIN = 5.0f;
constexpr auto SLOT_COUNT = game::component::HotbarComponent::SLOT_COUNT;

engine::render::Image makeFallbackPanelImage() {
    engine::render::Image image(
        "assets/farm-rpg/UI/Inventory/Slots.png",
        engine::utils::Rect{glm::vec2{6.0f, 105.0f}, glm::vec2{164.0f, 28.0f}}
    );
    image.setNineSliceMargins(engine::render::NineSliceMargins{7, 7, 7, 6});
    return image;
}

engine::render::Image makeFallbackSlotImage() {
    engine::render::Image image(
        "assets/farm-rpg/UI/Inventory/Slots.png",
        engine::utils::Rect{glm::vec2{151.0f, 38.0f}, glm::vec2{18.0f, 18.0f}}
    );
    image.setNineSliceMargins(engine::render::NineSliceMargins{2, 2, 2, 3});
    return image;
}

engine::render::Image makeFallbackSelectedImage() {
    engine::render::Image image(
        "assets/farm-rpg/UI/Inventory/Slots.png",
        engine::utils::Rect{glm::vec2{119.0f, 6.0f}, glm::vec2{18.0f, 18.0f}}
    );
    image.setNineSliceMargins(engine::render::NineSliceMargins{4, 4, 4, 4});
    return image;
}
} // namespace

namespace game::ui {

HotbarUI::HotbarUI(engine::core::Context& context,
                   game::data::ItemCatalog* catalog,
                   glm::vec2 slot_size,
                   glm::vec2 slot_spacing)
    : UIElement(glm::vec2{0.0f, 0.0f}),
      context_(context),
      item_catalog_(catalog),
      slot_size_(slot_size),
      slot_spacing_(slot_spacing) {
    setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    setPivot({0.0f, 0.0f});
    slot_inventory_indices_.assign(SLOT_COUNT, -1);
    buildLayout();
    context_.getInputManager()
        .onAction(entt::hashed_string{"mouse_right"}.value(), engine::input::ActionState::PRESSED)
        .connect<&HotbarUI::onMouseRightPressed>(this);
    show(); // 快捷栏默认显示
}

HotbarUI::~HotbarUI() {
    context_.getInputManager()
        .onAction(entt::hashed_string{"mouse_right"}.value(), engine::input::ActionState::PRESSED)
        .disconnect<&HotbarUI::onMouseRightPressed>(this);
}

void HotbarUI::buildLayout() {
    createPanel();
    createSlots();
}

glm::vec2 HotbarUI::calculatePanelSize() const {
    const float total_width = SLOT_COUNT * slot_size_.x +
                             (SLOT_COUNT - 1) * slot_spacing_.x +
                             panel_padding_.width();
    const float total_height = slot_size_.y + panel_padding_.height();

    return {total_width, total_height};
}

void HotbarUI::createPanel() {
    auto& preset_manager = context_.getResourceManager().getUIPresetManager();
    const auto* panel_preset = preset_manager.getImagePreset(QUICK_BAR_PANEL_PRESET.value());
    engine::render::Image panel_skin = panel_preset ? *panel_preset : makeFallbackPanelImage();

    const glm::vec2 panel_size = calculatePanelSize();
    const auto logical_size = context_.getGameState().getLogicalSize();

    // 快捷栏固定在屏幕底部中央
    const glm::vec2 panel_pos{
        (logical_size.x - panel_size.x) * 0.5f,
        logical_size.y - panel_size.y - BOTTOM_MARGIN
    };

    auto panel = std::make_unique<engine::ui::UIPanel>(panel_pos, panel_size, std::nullopt, panel_skin);
    panel->setPadding(panel_padding_);
    panel->setOrderIndex(5); // 比物品栏低的层级

    panel_ = panel.get();
    addChild(std::move(panel));
}

void HotbarUI::createSlots() {
    if (!panel_) {
        spdlog::error("HotbarUI: 面板未创建，无法生成槽位。");
        return;
    }

    auto& preset_manager = context_.getResourceManager().getUIPresetManager();
    const auto* slot_preset = preset_manager.getImagePreset(QUICK_BAR_SLOT_PRESET.value());
    const auto* selected_preset = preset_manager.getImagePreset(QUICK_BAR_SELECTED_PRESET.value());

    engine::render::Image slot_bg = slot_preset ? *slot_preset : makeFallbackSlotImage();
    engine::render::Image selected_bg = selected_preset ? *selected_preset : makeFallbackSelectedImage();

    // 创建水平栈布局
    auto stack_layout = std::make_unique<engine::ui::UIStackLayout>(glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f});
    stack_layout->setOrientation(engine::ui::Orientation::Horizontal);
    stack_layout->setSpacing(slot_spacing_.x);
    stack_layout->setContentAlignment(engine::ui::Alignment::Center);
    stack_layout->setAutoResize(true);

    slots_.reserve(static_cast<size_t>(SLOT_COUNT));

    for (auto i = 0; i < SLOT_COUNT; ++i) {
        auto slot = std::make_unique<engine::ui::UIItemSlot>(context_, glm::vec2{0.0f, 0.0f}, slot_size_);
        slot->setBackgroundImage(slot_bg);
        slot->setSelectionImage(selected_bg);
        slot->setSize(slot_size_);
        slot->setId(entt::hashed_string{"hotbar_slot"}.value());

        auto drag = std::make_unique<engine::ui::DragBehavior>();
        drag->setOnBegin([this, i](engine::ui::UIInteractive& owner, const glm::vec2&) { handleDragBegin(i, owner); });
        drag->setOnUpdate([this](engine::ui::UIInteractive& owner, const glm::vec2& pos, const glm::vec2&) {
            handleDragUpdate(owner, pos);
        });
        drag->setOnEnd([this, i](engine::ui::UIInteractive& owner, const glm::vec2& pos, bool) {
            handleDragEnd(i, owner, pos);
        });
        slot->addBehavior(std::move(drag));

        auto hover = std::make_unique<engine::ui::HoverBehavior>();
        hover->setOnEnter([this, i](engine::ui::UIInteractive&) { handleHoverEnter(i); });
        hover->setOnExit([this](engine::ui::UIInteractive&) { handleHoverExit(); });
        slot->addBehavior(std::move(hover));

        slots_.push_back(slot.get());
        stack_layout->addChild(std::move(slot));
    }

    stack_layout_ = stack_layout.get();
    panel_->addChild(std::move(stack_layout));
}

void HotbarUI::setSlotItem(int slot_index, const engine::ui::SlotItem& item) {
    if (slot_index < 0 || slot_index >= static_cast<int>(slots_.size())) return;
    slots_[slot_index]->setSlotItem(item);
}

void HotbarUI::clearSlot(int slot_index) {
    if (slot_index < 0 || slot_index >= static_cast<int>(slots_.size())) return;
    slots_[slot_index]->clearSlotItem();
}

void HotbarUI::clearAllSlots() {
    for (auto* slot : slots_) {
        if (slot) slot->clearSlotItem();
    }
}

void HotbarUI::setSlotInventoryIndex(int slot_index, int inventory_index) {
    if (slot_index < 0 || slot_index >= static_cast<int>(slot_inventory_indices_.size())) return;
    slot_inventory_indices_[static_cast<std::size_t>(slot_index)] = inventory_index;
}

void HotbarUI::resetInventoryMappings() {
    std::fill(slot_inventory_indices_.begin(), slot_inventory_indices_.end(), -1);
}

int HotbarUI::findSlotIndex(const engine::ui::UIItemSlot* slot) const {
    auto it = std::ranges::find(slots_, slot);
    return it != slots_.end() ? static_cast<int>(std::distance(slots_.begin(), it)) : -1;
}

void HotbarUI::setActiveSlot(int slot_index) {
    // slot_index == -1 表示清空高亮
    if (slot_index == -1) {
        if (active_slot_index_ >= 0 && active_slot_index_ < static_cast<int>(slots_.size())) {
            slots_[active_slot_index_]->setSelected(false);
        }
        active_slot_index_ = -1;
        return;
    }

    if (slot_index < 0 || slot_index >= static_cast<int>(slots_.size())) return;

    // 取消之前选中槽位的选中状态
    if (active_slot_index_ >= 0 && active_slot_index_ < static_cast<int>(slots_.size())) {
        slots_[active_slot_index_]->setSelected(false);
    }

    // 设置新的选中槽位
    active_slot_index_ = slot_index;
    slots_[active_slot_index_]->setSelected(true);
}

std::optional<engine::ui::SlotItem> HotbarUI::getSlotItemData(int slot_index) const {
    if (slot_index < 0 || slot_index >= static_cast<int>(slots_.size())) return std::nullopt;
    if (!slots_[slot_index]) return std::nullopt;
    const auto maybe = slots_[slot_index]->getSlotItem();
    if (!maybe) return std::nullopt;
    return maybe;
}

void HotbarUI::handleDragBegin(int slot_index, engine::ui::UIInteractive& owner) {
    dragging_item_.reset();
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
    if (slot_index < 0 || slot_index >= static_cast<int>(slot_inventory_indices_.size())) {
        dragging_ = false;
        game::ui::helpers::endDragPreview(ui_manager_);
        return;
    }
    const int mapped = slot_inventory_indices_[static_cast<std::size_t>(slot_index)];
    auto item = getSlotItemData(slot_index);
    dragging_ = mapped >= 0 && target_ != entt::null && item.has_value();
    dragging_slot_ = dragging_ ? slot_index : -1;
    dragging_inventory_slot_ = dragging_ ? mapped : -1;
    if (dragging_ && item) {
        dragging_item_ = item;
        const auto* slot = (slot_index >= 0 && slot_index < static_cast<int>(slots_.size())) ? slots_[slot_index] : nullptr;
        const glm::vec2 preview_size = game::ui::helpers::resolveDragPreviewSize(slot, slot_size_);
        const auto start_pos = owner.getContext().getInputManager().getLogicalMousePosition();
        game::ui::helpers::beginDragPreview(ui_manager_, item->icon, item->count, preview_size, start_pos);
        if (slot_index >= 0 && slot_index < static_cast<int>(slots_.size()) && slots_[slot_index]) {
            slots_[slot_index]->clearSlotItem();
        }
    } else {
        game::ui::helpers::endDragPreview(ui_manager_);
    }
}

bool HotbarUI::handleDropOnHotbar(int dst_hotbar_index) {
    if (dst_hotbar_index == dragging_slot_) {
        return true; // restore source
    }

    auto& dispatcher = context_.getDispatcher();
    const int dst_mapped = slot_inventory_indices_[static_cast<std::size_t>(dst_hotbar_index)];
    if (dst_mapped >= 0) {
        dispatcher.trigger(game::defs::HotbarBindRequest{target_, dst_hotbar_index, dragging_inventory_slot_});
        dispatcher.trigger(game::defs::HotbarBindRequest{target_, dragging_slot_, dst_mapped});
    } else {
        dispatcher.trigger(game::defs::HotbarBindRequest{target_, dst_hotbar_index, dragging_inventory_slot_});
        dispatcher.trigger(game::defs::HotbarUnbindRequest{target_, dragging_slot_});
    }
    return false;
}

bool HotbarUI::handleDropOnInventory(engine::ui::UIItemSlot* item_slot) {
    if (!inventory_ui_) {
        return true;
    }
    const int inventory_index = inventory_ui_->resolveInventoryIndex(item_slot);
    if (inventory_index < 0 || inventory_index == dragging_inventory_slot_) {
        return true;
    }
    context_.getDispatcher().trigger(game::defs::InventoryMoveRequest{target_, dragging_inventory_slot_, inventory_index, true});
    return false;
}

void HotbarUI::handleDragEnd(int slot_index, engine::ui::UIInteractive& owner, const glm::vec2& pos) {
    auto* target = findTarget(owner, pos);
    const int target_hotbar_index = target ? findSlotIndex(dynamic_cast<engine::ui::UIItemSlot*>(target)) : -1;

    auto finishDrag = [&]() {
        game::ui::helpers::endDragPreview(ui_manager_);
        dragging_ = false;
        dragging_slot_ = -1;
        dragging_inventory_slot_ = -1;
        dragging_item_.reset();
    };

    if (!dragging_ || slot_index != dragging_slot_ || target_ == entt::null) {
        if (target_ != entt::null && target_hotbar_index >= 0) {
            context_.getDispatcher().trigger(game::defs::HotbarActivateRequest{target_, target_hotbar_index});
        }
        finishDrag();
        return;
    }

    bool restore_source = false;

    if (target == nullptr) {
        context_.getDispatcher().trigger(game::defs::HotbarUnbindRequest{target_, dragging_slot_});
    } else if (auto* item_slot = dynamic_cast<engine::ui::UIItemSlot*>(target)) {
        restore_source = target_hotbar_index >= 0
            ? handleDropOnHotbar(target_hotbar_index)
            : handleDropOnInventory(item_slot);
    } else {
        context_.getDispatcher().trigger(game::defs::HotbarUnbindRequest{target_, dragging_slot_});
    }

    if (restore_source && dragging_item_ &&
        dragging_slot_ >= 0 && dragging_slot_ < static_cast<int>(slots_.size()) && slots_[dragging_slot_]) {
        slots_[dragging_slot_]->setSlotItem(*dragging_item_);
    }

    if (target_hotbar_index >= 0) {
        context_.getDispatcher().trigger(game::defs::HotbarActivateRequest{target_, target_hotbar_index});
    }

    finishDrag();
}

void HotbarUI::handleDragUpdate(engine::ui::UIInteractive& owner, const glm::vec2& pos) {
    (void)owner;
    if (!dragging_) return;
    if (ui_manager_) {
        ui_manager_->updateDragPreview(pos);
    }
}

void HotbarUI::handleHoverEnter(int slot_index) {
    if (!tooltip_ui_ || !item_catalog_ || dragging_) {
        return;
    }
    const auto item = getSlotItemData(slot_index);
    if (!item) {
        tooltip_ui_->hideTooltip();
        return;
    }

    const auto* data = item_catalog_->findItem(item->item_id);
    if (!data) {
        tooltip_ui_->hideTooltip();
        return;
    }

    tooltip_ui_->showItem(data->display_name_, data->category_str_, data->description_);
}

void HotbarUI::handleHoverExit() {
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
}

engine::ui::UIInteractive* HotbarUI::findTarget(engine::ui::UIInteractive& owner, const glm::vec2& pos) const {
    return game::ui::helpers::findTarget(ui_manager_, owner, pos);
}

void HotbarUI::show() {
    setVisible(true);
}

void HotbarUI::hide() {
    setVisible(false);
}

void HotbarUI::toggle() {
    setVisible(!isVisible());
}

bool HotbarUI::onMouseRightPressed() {
    if (!isVisible()) return false;

    const auto mouse_pos = context_.getInputManager().getLogicalMousePosition();
    auto* interactive = findInteractiveAt(mouse_pos);
    auto* item_slot = dynamic_cast<engine::ui::UIItemSlot*>(interactive);
    if (!item_slot) return false;

    // 右键点在槽位区域：一律吞掉，避免触发世界里的“取消选择”
    const int hotbar_index = findSlotIndex(item_slot);
    if (hotbar_index < 0 || hotbar_index >= static_cast<int>(slot_inventory_indices_.size())) {
        return true;
    }

    const int inventory_index = slot_inventory_indices_[static_cast<std::size_t>(hotbar_index)];
    if (inventory_index < 0 || target_ == entt::null) {
        return true;
    }

    const auto slot_item = item_slot->getSlotItem();
    if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        return true;
    }

    bool can_use = false;
    if (item_catalog_) {
        if (const auto* data = item_catalog_->findItem(slot_item->item_id)) {
            can_use = data->on_use_.has_value();
        }
    }

    if (can_use) {
        context_.getDispatcher().trigger(game::defs::UseItemRequest{target_, inventory_index, 1, false});
    }

    return true;
}

} // namespace game::ui
