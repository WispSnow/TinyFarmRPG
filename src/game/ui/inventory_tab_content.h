#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/ui/inventory_action_menu_model.h"
#include "game/ui/inventory_slot_drag_controller.h"
#include "game/ui/menu_tab_content.h"
#include "game/ui/slot_grid_support.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
class Element;
class Event;
}

namespace engine::core {
class Context;
}

namespace game::data {
struct ItemData;
class ItemCatalog;
}

namespace game::defs {
struct InventoryChanged;
struct HotbarChanged;
}

namespace game::ui {

class ItemTooltipUI;

/**
 * @brief 背包菜单标签页的 UI 控制器（MVVM）。
 *
 * 负责将玩家的 InventoryComponent / HotbarComponent 数据映射为 RmlUi 数据模型，
 * 并处理背包槽位与快捷栏槽位上的全部交互逻辑：
 *
 * - **数据绑定**：`bindModel()` 向 RmlUi 注册 backpack_slots、hotbar_slots、
 *   详情面板字段及上下文操作菜单，并将所有槽位事件回调绑定到对应处理函数。
 * - **数据同步**：监听 InventoryChanged / HotbarChanged ECS 事件，
 *   按需全量或增量刷新槽位 ViewModel；激活时调用 sync* 方法完成首次同步。
 * - **槽位交互**：
 *   - 左键点击：选中槽位；快捷栏还额外触发 HotbarActivateCommand。
 *   - 右键点击：弹出上下文操作菜单（Use / Discard / Activate / Unbind）。
 *   - 悬停：通过 ItemTooltipUI 显示物品提示框。
 * - **拖放**：背包↔背包触发 InventoryMoveCommand；背包→快捷栏触发
 *   HotbarBindCommand；快捷栏槽位拖出空白区触发 HotbarUnbindCommand；
 *   背包/快捷栏拖到垃圾桶会打开丢弃确认。
 * - **操作菜单**：浮动上下文菜单，定位时自动检测边界防止溢出，
 *   选择动作后通过 ECS Dispatcher 分发对应命令。
 *
 * 生命周期由 MenuScene 管理：`onActivated()` 时连接事件监听器，
 * `onDeactivated()` 时断开并清理所有瞬态 UI 状态。
 */
class InventoryTabContent final : public IMenuTabContent {
public:
    using ActorTargetRequestHandler = std::function<void(int inventory_slot_index)>;

private:
    // --- 外部依赖 ---
    engine::core::Context& context_;
    engine::ui::rmlui::RmlDocumentController& document_controller_;
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    uint64_t owner_scene_id_{0};

    // --- RmlUi 数据模型绑定字段 ---
    std::vector<SlotGridViewModel> backpack_slots_{};
    std::vector<SlotGridViewModel> hotbar_slots_{};

    // --- 悬停 / 提示框状态 ---
    std::unique_ptr<ItemTooltipUI> tooltip_ui_{};
    int hovered_slot_index_{-1};
    int hovered_hotbar_index_{-1};

    // --- 详情面板状态 ---
    Rml::String detail_name_{};
    Rml::String detail_category_{};
    Rml::String detail_description_{};
    bool has_detail_{false};
    SelectedSlot selected_slot_{};

    // --- 拖放状态 ---
    InventorySlotDragController drag_controller_{};

    // --- 操作菜单状态 ---
    InventoryActionMenuModel action_menu_model_{};
    bool listeners_connected_{false}; ///< 防止重复连接 ECS 事件监听器。
    ActorTargetRequestHandler actor_target_request_handler_{};

public:
    InventoryTabContent(engine::core::Context& context,
                        engine::ui::rmlui::RmlDocumentController& document_controller,
                        entt::registry& game_registry,
                        entt::entity player,
                        game::data::ItemCatalog* item_catalog,
                        uint64_t owner_scene_id);
    ~InventoryTabContent() override;

    /// 向 RmlUi 数据模型注册所有字段与事件回调；仅在文档加载时调用一次。
    [[nodiscard]] bool bindModel(Rml::DataModelConstructor& constructor) override;
    /// 标签页激活时：连接 ECS 事件监听器，同步背包与快捷栏数据，刷新整个模型。
    void onActivated() override;
    /// 标签页停用时：断开 ECS 事件监听器，关闭操作菜单、提示框，清除选中与拖拽状态。
    void onDeactivated() override;
    void update(float delta_time) override;
    /// 若操作菜单开启则关闭并消费取消事件，否则返回 false 交由上层处理。
    [[nodiscard]] bool onCancel() override;
    void setActorTargetRequestHandler(ActorTargetRequestHandler handler);

private:
    // --- 槽位索引校验 / 数据查询 ---

    [[nodiscard]] bool isValidPanelIndex(MenuPanelKind kind, int slot_index) const;
    [[nodiscard]] bool isPanelEmpty(MenuPanelKind kind, int slot_index) const;
    /// 将快捷栏槽位索引解析为对应的背包槽位索引；槽位为空则返回 -1。
    [[nodiscard]] int resolveInventorySlotFromHotbar(int hotbar_index) const;
    /// 将面板槽位坐标统一转换为背包槽位索引；快捷栏通过间接引用解析。
    [[nodiscard]] int resolveInventorySlotForPanel(MenuPanelKind kind, int slot_index) const;
    [[nodiscard]] const game::data::ItemData* resolveItemForPanel(MenuPanelKind kind, int slot_index) const;

    // --- 初始化 / 事件监听 ---

    void ensureTooltip();
    void disconnectRuntimeListeners();

    // --- 数据同步 ---

    /// 将整个 InventoryComponent 全量写入 backpack_slots_ ViewModel。
    void syncFromInventory();
    /// 将 HotbarComponent（间接引用 InventoryComponent）写入 hotbar_slots_ ViewModel。
    void syncHotbarFromInventory();
    /// 增量刷新单个背包槽位（由 InventoryChanged 事件驱动）。
    void refreshSlot(int slot_index);
    void markSlotsDirty();

    // --- 提示框 ---

    /// 拖拽或操作菜单开启时不显示提示框。
    void showTooltipForPanel(MenuPanelKind kind, int slot_index);
    void clearTooltip();

    // --- 详情面板 ---

    void setDetailFromItem(const game::data::ItemData& item);
    void updateDetailForPanel(MenuPanelKind kind, int slot_index);
    void clearDetail();

    // --- 选中状态 ---

    void selectSlot(MenuPanelKind kind, int slot_index);
    void clearSelection();
    void clearSelectionAndDetail();

    // --- 拖放 ---

    void clearDragState();

    // --- 操作菜单 ---

    void closeActionMenu();
    /// 背包槽位右键：提供 Use（有 on_use）/ Discard / Cancel 选项。
    void openBackpackActionMenu(int slot_index);
    /// 快捷栏槽位右键：提供 Activate / Use（有 on_use）/ Unbind / Cancel 选项。
    void openHotbarActionMenu(int slot_index);
    /// 二次确认丢弃，展示"Discard x[N]? / Cancel"操作菜单。
    void openDiscardConfirmForBackpackSlot(int slot_index);
    /// 填充操作菜单模型、强制 RmlUi 更新后定位菜单至锚点槽位旁。
    void showActionMenu(Rml::String title,
                        std::vector<InventoryActionEntryViewModel> entries,
                        std::string_view anchor_grid_id,
                        int anchor_slot_index);
    /// 将操作菜单定位在锚点槽位右侧；若右侧溢出则改为左侧，最终 clamp 至区域内。
    void positionActionMenuForGridSlot(std::string_view grid_id, int slot_index);
    [[nodiscard]] Rml::Element* findIndexedChildElement(std::string_view parent_id, int child_index) const;
    /// 通过测量相邻槽位的绝对坐标差计算网格水平间距，用于操作菜单定位偏移。
    [[nodiscard]] float measureGridHorizontalGap(std::string_view grid_id) const;
    /// 根据 action_id 分发 ECS 命令（Use / Discard / Activate / Unbind / ConfirmDiscard）。
    void executeAction(int action_id);

    // --- 工具栏按钮回调 ---

    void onTrashClicked();
    void onTrashDragDrop(Rml::Event& event);
    void onSortClicked();

    // --- ECS 事件处理 ---

    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void onHotbarChanged(const game::defs::HotbarChanged& evt);

    // --- 槽位交互事件回调 ---

    void onSlotFocus(MenuPanelKind kind, int slot_index, Rml::Event& event);
    /// 右键按下：阻止冒泡，选中槽位，对非空槽开启操作菜单。
    void onSlotMouseDown(MenuPanelKind kind, int slot_index, Rml::Event& event);
    /// 左键抬起：选中槽位；快捷栏额外触发 HotbarActivateCommand。
    void onSlotMouseUp(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotHoverEnter(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotHoverExit(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotDragStart(MenuPanelKind kind, int slot_index, Rml::Event& event);
    /// 拖放落点处理：背包→背包触发 InventoryMoveCommand，*→快捷栏触发 HotbarBindCommand。
    void onSlotDragDrop(MenuPanelKind kind, int slot_index, Rml::Event& event);
    /// 拖放结束于原始槽位（未 drop）时，若为快捷栏则触发 HotbarUnbindCommand。
    void onSlotDragEnd(MenuPanelKind kind, int slot_index, Rml::Event& event);

    void onActionEntryClick(int action_id, Rml::Event& event);
};

} // namespace game::ui
