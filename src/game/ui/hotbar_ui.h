#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "engine/ui/ui_types.h"
#include "game/data/item_catalog.h"
#include "game/defs/events.h"
#include "game/ui/slot_grid_support.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
class Event;
}

namespace engine::core {
class Context;
}

namespace engine::ui::rmlui {
class RmlUiRuntime;
}

namespace game::ui {

class ItemTooltipUI;

/// @brief 游戏内 HUD 快捷栏控制器。
/// @details
/// - 持有并驱动 `ui/rmlui/hud/hotbar.rml` 文档。
/// - 维护一份适合 RmlUi data model 绑定的 `hotbar_slots_` 视图模型。
/// - 将 UI 交互翻译成 gameplay 命令：左键激活、右键使用、拖拽换绑/解绑。
/// - 拖拽视觉和命中检测由 RmlUi 原生 `drag: clone` 处理；本类只解释事件结果。
class HotbarUI final {
    engine::ui::rmlui::RmlUiRuntime& runtime_;
    engine::core::Context& context_;
    game::data::ItemCatalog* item_catalog_{nullptr};
    uint64_t owner_scene_id_{0};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};

    std::vector<SlotGridViewModel> hotbar_slots_{};         ///< RmlUi 直接绑定的视图模型数组。
    std::vector<std::optional<engine::ui::SlotItem>> slot_items_{}; ///< 每个快捷栏槽位当前显示的物品快照，用于生成图标/数量文本。
    std::vector<int> slot_inventory_indices_{};     ///< 快捷栏槽位到背包槽位的映射，用于激活/使用/拖拽换绑时发布命令。
    entt::entity target_{entt::null};
    int active_slot_index_{-1};
    int hovered_slot_index_{-1};
    bool visible_{true};
    bool data_types_registered_{false};

    game::ui::ItemTooltipUI* tooltip_ui_{nullptr};

    /// 只保留“是否正在拖拽 / 是否已落点处理”这类最小状态；
    /// 拖拽来源槽位由 RmlUi 事件中的 `drag_element` 属性提供。
    SlotGridDragState drag_state_{};

public:
    HotbarUI(engine::ui::rmlui::RmlUiRuntime& runtime,
             engine::core::Context& context,
             uint64_t owner_scene_id,
             game::data::ItemCatalog* catalog = nullptr);
    ~HotbarUI();

    HotbarUI(const HotbarUI&) = delete;
    HotbarUI& operator=(const HotbarUI&) = delete;
    HotbarUI(HotbarUI&&) = delete;
    HotbarUI& operator=(HotbarUI&&) = delete;

    [[nodiscard]] bool isReady() const {
        return document_controller_.document() != nullptr && document_controller_.isModelValid();
    }
    [[nodiscard]] bool isVisible() const { return visible_; }

    void setSlotItem(int slot_index, const engine::ui::SlotItem& item);
    void clearSlot(int slot_index);
    void clearAllSlots();
    void setActiveSlot(int slot_index);
    [[nodiscard]] int getActiveSlotIndex() const { return active_slot_index_; }

    void setTooltipUI(game::ui::ItemTooltipUI* tooltip_ui) { tooltip_ui_ = tooltip_ui; }
    /// @brief 设置当前快捷栏交互命令的目标实体，通常是玩家。
    /// @details 左键激活、右键使用、拖拽换绑/解绑都会把该实体写入 dispatcher 命令。
    void setTarget(entt::entity target) { target_ = target; }

    /// @brief 设置快捷栏槽位到背包槽位的映射关系。
    /// @details
    /// HotbarUI 显示的是“快捷栏第 N 格”，但实际物品数据和使用逻辑仍来自背包槽位。
    /// 该映射用于：
    /// - 判断该快捷栏槽位是否可拖拽；
    /// - 将右键使用翻译为 `UseItemCommand(target_, inventory_index, ...)`；
    /// - 在快捷栏拖拽时发布正确的 bind / unbind 命令。
    void setSlotInventoryIndex(int slot_index, int inventory_index);
    void resetInventoryMappings();

    void show();
    void hide();
    void toggle();

    /// @brief 同步整份快捷栏状态快照到 HUD。
    /// @details
    /// 这是一个“纯 UI 状态同步”接口，而不是 dispatcher 事件回调。
    /// 调用方负责先拆开 gameplay 事件，再把必要字段传进来。
    void syncState(entt::entity target,
                   bool full_sync,
                   int active_slot,
                   std::span<const game::defs::HotbarSlotUpdate> slot_updates);
    /// @brief 同步当前激活槽位到 HUD 高亮。
    /// @details 同样由上层把事件拆散后调用，避免 HotbarUI 暴露出事件回调风格签名。
    void syncActiveSlot(entt::entity target, int slot_index);

private:
    /// 创建 data model、绑定事件、加载 RML 文档。
    [[nodiscard]] bool initDocument();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    void destroyDocument();

    [[nodiscard]] bool isValidSlotIndex(int slot_index) const;
    void refreshAllSlotViewModels();
    /// 将缓存的 gameplay 快照转换为单个 RmlUi 槽位视图模型。
    void refreshSlotViewModel(int slot_index);
    void markSlotsDirty();

    [[nodiscard]] std::optional<engine::ui::SlotItem> getSlotItemData(int slot_index) const;

    void showTooltipForSlot(int slot_index);
    void refreshTooltipForHoveredSlot();
    void clearTooltip();
    void clearDragState();

    /// 左键：激活快捷栏；右键：若物品可用，则对映射背包槽位发布 UseItemCommand。
    void onSlotMouseUp(int slot_index, Rml::Event& event);
    void onSlotHoverEnter(int slot_index, Rml::Event& event);
    void onSlotHoverExit(int slot_index, Rml::Event& event);
    /// 仅在拖拽开始时记录最小状态，具体来源槽位由 RmlUi 拖拽元素属性携带。
    void onSlotDragStart(int slot_index, Rml::Event& event);
    /// 根据 drop 目标与来源槽位，翻译为快捷栏换绑/解绑命令。
    void onSlotDragDrop(int slot_index, Rml::Event& event);
    /// 若拖拽结束时未命中任何可接受的 drop 目标，则把当前快捷栏槽位解绑。
    void onSlotDragEnd(int slot_index, Rml::Event& event);
};

} // namespace game::ui
