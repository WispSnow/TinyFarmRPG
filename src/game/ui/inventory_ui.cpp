#include "inventory_ui.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/ui_draggable_panel.h"
#include "engine/ui/layout/ui_grid_layout.h"
#include "engine/ui/ui_panel.h"
#include "engine/ui/ui_preset_manager.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_label.h"
#include "engine/ui/behavior/drag_behavior.h"
#include "engine/ui/behavior/hover_behavior.h"
#include "engine/ui/ui_manager.h"
#include "engine/input/input_manager.h"
#include "game/ui/hotbar_ui.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/ui_drag_drop_helpers.h"
#include "game/component/inventory_component.h"
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <string>
#include <string_view>

namespace {
constexpr entt::hashed_string INVENTORY_PANEL_PRESET{"inventory_panel"};
constexpr entt::hashed_string INVENTORY_SLOT_PRESET{"inventory_slot"};
constexpr float RIGHT_MARGIN = 20.0f;
constexpr float BUTTON_AREA_HEIGHT = 16.0f;

engine::render::Image makeFallbackPanelImage() {
    engine::render::Image image(
        "assets/farm-rpg/UI/Inventory/inventory.png",
        engine::utils::Rect{glm::vec2{0.0f, 64.0f}, glm::vec2{48.0f, 48.0f}}
    );
    image.setNineSliceMargins(engine::render::NineSliceMargins{10, 10, 10, 10});
    return image;
}

engine::render::Image makeFallbackSlotImage() {
    engine::render::Image image(
        "assets/farm-rpg/UI/Inventory/inventory.png",
        engine::utils::Rect{glm::vec2{39.0f, 9.0f}, glm::vec2{18.0f, 18.0f}}
    );
    image.setNineSliceMargins(engine::render::NineSliceMargins{2, 2, 2, 2});
    return image;
}
} // namespace

namespace game::ui {

InventoryUI::InventoryUI(engine::core::Context& context,
                         game::data::ItemCatalog* catalog,
                         int columns,
                         int rows,
                         glm::vec2 slot_size,
                         glm::vec2 slot_spacing)
    : UIElement(glm::vec2{0.0f, 0.0f}),
      context_(context),
      item_catalog_(catalog),
      columns_(columns),
      rows_(rows),
      slot_size_(slot_size),
      slot_spacing_(slot_spacing) {
    setAnchor({0.0f, 0.0f}, {1.0f, 1.0f});
    setPivot({0.0f, 0.0f});
    slot_items_.resize(static_cast<std::size_t>(columns_ * rows_ * game::component::InventoryComponent::PAGE_COUNT)); // 多页缓存（与 InventoryComponent 对齐）
    buildLayout();
    subscribeEvents();
    context_.getInputManager()
        .onAction(entt::hashed_string{"mouse_right"}.value(), engine::input::ActionState::PRESSED)
        .connect<&InventoryUI::onMouseRightPressed>(this);
    hide(); // 默认隐藏，按 inventory 动作显示
}

InventoryUI::~InventoryUI() {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<game::defs::InventoryChanged>().disconnect<&InventoryUI::onInventoryChanged>(this);
    context_.getInputManager()
        .onAction(entt::hashed_string{"mouse_right"}.value(), engine::input::ActionState::PRESSED)
        .disconnect<&InventoryUI::onMouseRightPressed>(this);
}

void InventoryUI::buildLayout() {
    createPanel();
    createCloseButton();
    createGridAndSlots();
    createPageButtons();
}

void InventoryUI::createCloseButton() {
    if (!panel_) return;

    constexpr float BUTTON_SIZE = 16.0f;
    auto close_button = engine::ui::UIButton::create(
        context_, "close", glm::vec2{0.0f, 0.0f}, glm::vec2{BUTTON_SIZE, BUTTON_SIZE}, [this]() {
            if (tooltip_ui_) {
                tooltip_ui_->hideTooltip();
            }
            if (ui_manager_) {
                ui_manager_->endDragPreview();
            }
            dragging_ = false;
            dragging_inventory_index_ = -1;
            dragging_item_.reset();
            hide();
        });
    if (!close_button) {
        spdlog::error("InventoryUI: 创建 close button 失败。");
        return;
    }
    close_button->setAnchor({1.0f, 0.0f}, {1.0f, 0.0f});
    close_button->setPivot({0.8f, 0.2f});
    panel_->addChild(std::move(close_button), 100);
}

int InventoryUI::getTotalPages() const {
    const int page_size = columns_ * rows_;
    if (page_size <= 0) return 1;
    const int total = static_cast<int>((slot_items_.size() + static_cast<std::size_t>(page_size) - 1u) /
                                       static_cast<std::size_t>(page_size));
    return std::max(1, total);
}

void InventoryUI::updatePageLabel() {
    if (!page_label_) return;
    const int total_pages = getTotalPages();
    const int clamped_page = std::clamp(current_page_, 0, std::max(0, total_pages - 1));
    page_label_->setText(std::to_string(clamped_page + 1) + "/" + std::to_string(total_pages));
}

glm::vec2 InventoryUI::calculateGridSize() const {
    glm::vec2 grid_size{
        columns_ * slot_size_.x + static_cast<float>(columns_ - 1) * slot_spacing_.x,
        rows_ * slot_size_.y + static_cast<float>(rows_ - 1) * slot_spacing_.y
    };
    return grid_size;
}

void InventoryUI::createPanel() {
    auto& preset_manager = context_.getResourceManager().getUIPresetManager();
    const auto* panel_preset = preset_manager.getImagePreset(INVENTORY_PANEL_PRESET.value());
    engine::render::Image panel_skin = panel_preset ? *panel_preset : makeFallbackPanelImage();
    auto panel_margins = panel_skin.getNineSliceMargins();

    const glm::vec2 grid_size = calculateGridSize();
    const glm::vec2 panel_size{
        grid_size.x + panel_padding_.width(),
        grid_size.y + panel_padding_.height() + BUTTON_AREA_HEIGHT
    };

    const auto logical_size = context_.getGameState().getLogicalSize();
    const glm::vec2 initial_pos{
        logical_size.x - panel_size.x - RIGHT_MARGIN,
        (logical_size.y - panel_size.y) * 0.5f
    };

    auto panel = std::make_unique<engine::ui::UIDraggablePanel>(
        context_, initial_pos, panel_size, std::nullopt, panel_skin, panel_margins);
    panel->setPadding(panel_padding_);
    panel->setOrderIndex(10);

    panel_ = panel.get();
    addChild(std::move(panel));
}

void InventoryUI::createGridAndSlots() {
    if (!panel_) {
        spdlog::error("InventoryUI: 面板未创建，无法生成网格。");
        return;
    }

    auto content_panel = panel_->getContentPanel();
    if (!content_panel) {
        spdlog::error("InventoryUI: 内容面板不存在。");
        return;
    }

    const glm::vec2 grid_size = calculateGridSize();

    auto grid = std::make_unique<engine::ui::UIGridLayout>(glm::vec2{0.0f, 0.0f}, grid_size);
    grid->setColumnCount(columns_);
    grid->setSpacing(slot_spacing_);
    grid->setCellSize(slot_size_);
    grid->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});

    auto& preset_manager = context_.getResourceManager().getUIPresetManager();
    const auto* slot_preset = preset_manager.getImagePreset(INVENTORY_SLOT_PRESET.value());
    engine::render::Image slot_bg = slot_preset ? *slot_preset : makeFallbackSlotImage();

    const int total_slots = columns_ * rows_;
    slots_.reserve(static_cast<size_t>(total_slots));

    for (int i = 0; i < total_slots; ++i) {
        auto slot = std::make_unique<engine::ui::UIItemSlot>(context_, glm::vec2{0.0f, 0.0f}, slot_size_);
        slot->setBackgroundImage(slot_bg);
        slot->setSize(slot_size_);
        slot->setId(entt::hashed_string{"inventory_slot"}.value());
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
        grid->addChild(std::move(slot));
    }

    grid_ = grid.get();
    content_panel->addChild(std::move(grid));
}

void InventoryUI::createPageButtons() {
    if (!panel_) return;
    auto content_panel = panel_->getContentPanel();
    if (!content_panel) return;

    const glm::vec2 grid_size = calculateGridSize();
    constexpr float BUTTON_SIZE = 20.0f;
    constexpr float LABEL_BUTTON_GAP = 8.0f;

    const float extra_height = panel_padding_.bottom + BUTTON_AREA_HEIGHT - BUTTON_SIZE;
    const float half_gap_y = std::max(0.0f, extra_height * 0.5f);
    const float y = grid_size.y + half_gap_y;
    const float center_x = grid_size.x * 0.5f;

    const int total_pages = getTotalPages();
    const std::string label_text_max = std::to_string(total_pages) + "/" + std::to_string(total_pages);
    const std::string label_text = std::to_string(current_page_ + 1) + "/" + std::to_string(total_pages);

    auto page_label = std::make_unique<engine::ui::UILabel>(context_.getTextRenderer(),
                                                            label_text_max,
                                                            engine::ui::DEFAULT_UI_FONT_PATH,
                                                            engine::ui::DEFAULT_UI_FONT_SIZE_PX,
                                                            glm::vec2{center_x, y + BUTTON_SIZE * 0.5f},
                                                            engine::utils::FColor::black());
    const float label_width = page_label->getRequestedSize().x;
    page_label_ = page_label.get();
    page_label_->setPivot({0.5f, 0.5f});
    page_label_->setText(label_text);

    const float half_gap_x = label_width * 0.5f + LABEL_BUTTON_GAP;

    auto page_left = engine::ui::UIButton::create(
        context_, "page_left",
        glm::vec2{center_x - half_gap_x - BUTTON_SIZE, y},
        glm::vec2{BUTTON_SIZE, BUTTON_SIZE},
        [this]() { changePage(-1); });

    auto page_right = engine::ui::UIButton::create(
        context_, "page_right",
        glm::vec2{center_x + half_gap_x, y},
        glm::vec2{BUTTON_SIZE, BUTTON_SIZE},
        [this]() { changePage(1); });

    if (page_left) {
        content_panel->addChild(std::move(page_left));
    } else {
        spdlog::error("InventoryUI: 创建 page_left button 失败。");
    }
    if (page_right) {
        content_panel->addChild(std::move(page_right));
    } else {
        spdlog::error("InventoryUI: 创建 page_right button 失败。");
    }
    content_panel->addChild(std::move(page_label));
}

void InventoryUI::setSlotItem(int index, const engine::ui::SlotItem& item) {
    if (index < 0 || index >= static_cast<int>(slots_.size())) return;
    slots_[index]->setSlotItem(item);
}

void InventoryUI::clearSlot(int index) {
    if (index < 0 || index >= static_cast<int>(slots_.size())) return;
    slots_[index]->clearSlotItem();
}

void InventoryUI::clearAllSlots() {
    for (auto* slot : slots_) {
        if (slot) slot->clearSlotItem();
    }
}

void InventoryUI::show() {
    setVisible(true);
}

void InventoryUI::hide() {
    setVisible(false);
}

void InventoryUI::toggle() {
    setVisible(!isVisible());
}

bool InventoryUI::onMouseRightPressed() {
    if (!isVisible()) return false;

    const auto mouse_pos = context_.getInputManager().getLogicalMousePosition();
    auto* interactive = findInteractiveAt(mouse_pos);
    auto* item_slot = dynamic_cast<engine::ui::UIItemSlot*>(interactive);
    if (!item_slot) return false;

    // 右键点在槽位区域：一律吞掉，避免触发世界里的“取消选择”
    const int inventory_index = resolveInventoryIndex(item_slot);
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
        context_.getDispatcher().trigger(game::defs::UseItemRequest{target_, inventory_index, 1, true});
    }

    return true;
}

void InventoryUI::subscribeEvents() {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<game::defs::InventoryChanged>().connect<&InventoryUI::onInventoryChanged>(this);
}

void InventoryUI::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    if (evt.slots.empty()) return;

    for (const auto& slot : evt.slots) {
        if (slot.slot_index < 0 || slot.slot_index >= static_cast<int>(slot_items_.size())) continue;
        auto& cached = slot_items_[static_cast<std::size_t>(slot.slot_index)];
        cached.item_id = slot.item_id;
        cached.count = slot.count;
    }

    current_page_ = std::clamp(evt.active_page, 0, std::max(0, getTotalPages() - 1));
    updatePageLabel();
    renderPage(current_page_);
}

void InventoryUI::renderPage(int active_page) {
    const int page_size = columns_ * rows_;
    const int offset = active_page * page_size;
    if (offset < 0 || offset + page_size > static_cast<int>(slot_items_.size())) return;

    for (int i = 0; i < page_size; ++i) {
        const auto& cached = slot_items_[static_cast<std::size_t>(offset + i)];
        if (cached.item_id != entt::null && cached.count > 0) {
            if (item_catalog_) {
                setSlotItem(i, engine::ui::SlotItem{cached.item_id, cached.count, item_catalog_->getItemIcon(cached.item_id)});
            } else {
                setSlotItem(i, engine::ui::SlotItem{cached.item_id, cached.count, cached.icon});
            }
        } else {
            clearSlot(i);
        }
    }
}

void InventoryUI::changePage(int delta) {
    const int new_page = std::clamp(current_page_ + delta, 0, std::max(0, getTotalPages() - 1));
    if (new_page == current_page_) return;
    current_page_ = new_page;
    updatePageLabel();
    renderPage(current_page_);
    if (target_ != entt::null) {
        context_.getDispatcher().trigger(game::defs::InventorySetActivePageRequest{target_, current_page_});
    }
}

int InventoryUI::findSlotIndex(const engine::ui::UIItemSlot* slot) const {
    auto it = std::ranges::find(slots_, slot);
    return it != slots_.end() ? static_cast<int>(std::distance(slots_.begin(), it)) : -1;
}

int InventoryUI::resolveInventoryIndex(const engine::ui::UIItemSlot* slot) const {
    const int local_index = findSlotIndex(slot);
    if (local_index < 0) return -1;
    const int page_size = columns_ * rows_;
    return current_page_ * page_size + local_index;
}

std::optional<engine::ui::SlotItem> InventoryUI::getSlotItemData(int inventory_index) const {
    if (inventory_index < 0 || inventory_index >= static_cast<int>(slot_items_.size())) return std::nullopt;
    const auto& cached = slot_items_[static_cast<std::size_t>(inventory_index)];
    if (cached.item_id == entt::null || cached.count <= 0) return std::nullopt;
    if (item_catalog_) {
        return engine::ui::SlotItem{cached.item_id, cached.count, item_catalog_->getItemIcon(cached.item_id)};
    }
    return engine::ui::SlotItem{cached.item_id, cached.count, cached.icon};
}

void InventoryUI::handleDragBegin(int local_index, engine::ui::UIInteractive& owner) {
    dragging_item_.reset();
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
    const int page_size = columns_ * rows_;
    const int inventory_index = current_page_ * page_size + local_index;
    if (inventory_index < 0 || inventory_index >= static_cast<int>(slot_items_.size()) || target_ == entt::null) {
        dragging_ = false;
        game::ui::helpers::endDragPreview(ui_manager_);
        return;
    }

    auto item = getSlotItemData(inventory_index);
    dragging_ = item.has_value();
    dragging_inventory_index_ = dragging_ ? inventory_index : -1;
    if (dragging_ && item) {
        dragging_item_ = item;
        const auto* slot = (local_index >= 0 && local_index < static_cast<int>(slots_.size())) ? slots_[local_index] : nullptr;
        const glm::vec2 preview_size = game::ui::helpers::resolveDragPreviewSize(slot, slot_size_);
        const auto start_pos = owner.getContext().getInputManager().getLogicalMousePosition();
        game::ui::helpers::beginDragPreview(ui_manager_, item->icon, item->count, preview_size, start_pos);
        if (local_index >= 0 && local_index < static_cast<int>(slots_.size()) && slots_[local_index]) {
            slots_[local_index]->clearSlotItem();
        }
    } else {
        game::ui::helpers::endDragPreview(ui_manager_);
    }
}

void InventoryUI::handleDragEnd(int local_index, engine::ui::UIInteractive& owner, const glm::vec2& pos) {
    if (!dragging_ || target_ == entt::null) {
        dragging_ = false;
        dragging_inventory_index_ = -1;
        dragging_item_.reset();
        game::ui::helpers::endDragPreview(ui_manager_);
        return;
    }

    const int page_size = columns_ * rows_;
    const int from_index = dragging_inventory_index_;
    auto* target = findTarget(owner, pos);
    bool restore_source = true;

    // 拖拽成功的情况，目标可能是物品栏槽位或快捷栏槽位
    if (auto* item_slot = dynamic_cast<engine::ui::UIItemSlot*>(target)) {
        const int local_target = findSlotIndex(item_slot);
        if (local_target >= 0) {
            const int to_index = current_page_ * page_size + local_target;
            if (to_index != from_index) {
                restore_source = false;
                context_.getDispatcher().trigger(game::defs::InventoryMoveRequest{target_, from_index, to_index, true});
            }
        } else if (hotbar_ui_) {
            const int hotbar_index = hotbar_ui_->findSlotIndex(item_slot);
            if (hotbar_index >= 0) {
                context_.getDispatcher().trigger(game::defs::HotbarBindRequest{target_, hotbar_index, from_index});
            }
        }
    }

    // 拖拽失败的情况
    if (restore_source && dragging_item_ && local_index >= 0 &&
        local_index < static_cast<int>(slots_.size()) && slots_[local_index]) {
        slots_[local_index]->setSlotItem(*dragging_item_);
    }

    game::ui::helpers::endDragPreview(ui_manager_);
    dragging_ = false;
    dragging_inventory_index_ = -1;
    dragging_item_.reset();
}

engine::ui::UIInteractive* InventoryUI::findTarget(engine::ui::UIInteractive& owner, const glm::vec2& pos) const {
    return game::ui::helpers::findTarget(ui_manager_, owner, pos);
}

void InventoryUI::handleDragUpdate(engine::ui::UIInteractive& owner, const glm::vec2& pos) {
    (void)owner;
    if (!dragging_) return;
    if (ui_manager_) {
        ui_manager_->updateDragPreview(pos);
    }
}

void InventoryUI::handleHoverEnter(int local_index) {
    if (!tooltip_ui_ || !item_catalog_ || dragging_) {
        return;
    }
    const int page_size = columns_ * rows_;
    const int inventory_index = current_page_ * page_size + local_index;
    const auto item = getSlotItemData(inventory_index);
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

void InventoryUI::handleHoverExit() {
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
}

} // namespace game::ui
