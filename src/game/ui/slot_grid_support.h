#pragma once

#include "engine/input/mouse_cursor_service.h"
#include "engine/ui/ui_types.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Rml {
class DataModelConstructor;
class Event;
}

namespace game::data {
class ItemCatalog;
}

namespace game::ui {

// 标识菜单中可交互面板的种类（选中、焦点语境）。
// 与拖拽来源分离定义，避免“可选中但不可拖拽”的面板被错误耦合。
enum class MenuPanelKind {
    None = 0,
    Backpack,
    Hotbar,
};

// 单个槽位格子的 UI 数据快照，绑定到 RmlUi 数据模型供模板渲染。
struct SlotGridViewModel {
    int slot_index{0};
    Rml::String icon_decorator{"none"};
    Rml::String count_text{};
    Rml::String label{};
    bool has_item{false};
    bool has_count{false};
    bool can_drag{false};
    bool is_selected{false};
    bool is_active{false};
};

// 构建 SlotGridViewModel 时的附加开关，控制可拖拽、选中、激活状态与标签文字。
struct SlotGridViewModelOptions {
    bool can_drag{false};
    bool is_selected{false};
    bool is_active{false};
    std::string_view label{};
};

// 标识拖拽操作的发起来源（背包 / 快捷栏）。
enum class SlotGridDragSourceKind {
    None = 0,
    Inventory = 1,
    Hotbar = 2,
};

// 拖拽事件携带的最小 payload：来源类型 + 槽位索引。
struct SlotGridDragInfo {
    SlotGridDragSourceKind source{SlotGridDragSourceKind::None};
    int slot_index{-1};

    [[nodiscard]] bool fromInventory() const { return source == SlotGridDragSourceKind::Inventory; }
    [[nodiscard]] bool fromHotbar() const { return source == SlotGridDragSourceKind::Hotbar; }
};

// 单个面板的拖拽生命周期状态：是否活跃、drop 是否已被处理。
struct SlotGridDragState {
    explicit SlotGridDragState(engine::input::MouseCursorService* cursor_service = nullptr);

    void clear();
    void start();

    bool active{false};
    bool drop_handled{false};

private:
    engine::input::MouseCursorService* cursor_service_{nullptr};
    std::optional<engine::input::ScopedCursorOverride> cursor_override_{};
};

// 统一记录当前选中的槽位：面板种类 + 索引。
struct SelectedSlot {
    MenuPanelKind panel{MenuPanelKind::None};
    int index{-1};

    [[nodiscard]] bool valid() const {
        return panel != MenuPanelKind::None && index >= 0;
    }

    [[nodiscard]] bool isBackpack() const {
        return panel == MenuPanelKind::Backpack;
    }

    [[nodiscard]] bool isHotbar() const {
        return panel == MenuPanelKind::Hotbar;
    }

    void clear() {
        panel = MenuPanelKind::None;
        index = -1;
    }
};

// 向 RmlUi 注册 SlotGridViewModel 的字段类型，使其可被数据绑定读取。
[[nodiscard]] bool registerSlotGridViewModelType(Rml::DataModelConstructor& constructor);

// 根据可选物品数据与 UI 选项，填充单个槽位的视图模型。
void populateSlotGridViewModel(SlotGridViewModel& view_model,
                               const std::optional<engine::ui::SlotItem>& item,
                               game::data::ItemCatalog* item_catalog,
                               const SlotGridViewModelOptions& options = {});

// 从 RmlUi 回调的 arguments 中提取单个整数参数（槽位索引）。
// 对应 RML 写法例如：data-event-mouseup="slot_mouse_up(slot.slot_index)"，
// 其中 `slot` 来自 `data-for="slot : hotbar_slots"` / `data-for="slot : backpack_slots"` 的当前迭代项，
// 因此点击某一格时，RmlUi 会在该格对应的数据上下文里求值 `slot.slot_index`，
// 再把结果作为唯一参数放入 VariantList。
[[nodiscard]] int getSingleIntArgument(const Rml::VariantList& arguments);
// 从事件参数中解析拖拽来源信息，不存在时返回 nullopt。
[[nodiscard]] std::optional<SlotGridDragInfo> getSlotGridDragInfo(const Rml::Event& event);

// 统一的“按槽位索引分发”成员函数签名。
// 约定参数顺序为 (slot_index, event)。
// 注意：这不是 RmlUi 原生要求的签名，而是本项目在 slot-grid 场景下的简化适配层。
template<typename Owner>
using IndexedEventHandler = void (Owner::*)(int, Rml::Event&);

// 单个事件绑定描述：事件名 + 对应成员函数。
template<typename Owner>
struct IndexedEventBinding {
    std::string_view name;
    IndexedEventHandler<Owner> handler{nullptr};
};

// 槽位网格常见交互事件集合。
// 字段允许为空，表示该事件不注册。
template<typename Owner>
struct SlotGridEventHandlers {
    IndexedEventHandler<Owner> on_focus{nullptr};
    IndexedEventHandler<Owner> on_mouse_down{nullptr};
    IndexedEventHandler<Owner> on_mouse_up{nullptr};
    IndexedEventHandler<Owner> on_hover_enter{nullptr};
    IndexedEventHandler<Owner> on_hover_exit{nullptr};
    IndexedEventHandler<Owner> on_drag_start{nullptr};
    IndexedEventHandler<Owner> on_drag_drop{nullptr};
    IndexedEventHandler<Owner> on_drag_end{nullptr};
};

// 统一的“上下文 + 槽位索引”分发成员函数签名。
// 约定参数顺序为 (context, slot_index, event)。
template<typename Owner, typename Context>
using IndexedContextEventHandler = void (Owner::*)(Context, int, Rml::Event&);

/// 槽位网格常见交互事件集合，额外携带一个调用方定义的上下文。
///
/// 当同一组事件处理函数需要区分面板来源（如 Backpack vs Hotbar）时，
/// 使用此版本并以面板种类枚举作为 Context；不需要区分时使用 SlotGridEventHandlers。
template<typename Owner, typename Context>
struct SlotGridContextEventHandlers {
    IndexedContextEventHandler<Owner, Context> on_focus{nullptr};
    IndexedContextEventHandler<Owner, Context> on_mouse_down{nullptr};
    IndexedContextEventHandler<Owner, Context> on_mouse_up{nullptr};
    IndexedContextEventHandler<Owner, Context> on_hover_enter{nullptr};
    IndexedContextEventHandler<Owner, Context> on_hover_exit{nullptr};
    IndexedContextEventHandler<Owner, Context> on_drag_start{nullptr};
    IndexedContextEventHandler<Owner, Context> on_drag_drop{nullptr};
    IndexedContextEventHandler<Owner, Context> on_drag_end{nullptr};
};

// 绑定单个“带槽位索引”的事件回调。
// 若 owner 或 handler 为空，直接返回 false，避免悬空调用。
//
// RmlUi 原生 data event 回调签名是：
//   void(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
// 这里额外包了一层，把它适配成项目里更常用的：
//   void(slot_index, event)
//
// 例如：
// - RML: data-event-mouseup="slot_mouse_up(slot.slot_index)"
// - 注册名: "slot_mouse_up"
// - arguments: { slot.slot_index }
// - 最终调用: owner->onSlotMouseUp(slot_index, event)
template<typename Owner>
[[nodiscard]] bool bindIndexedEventCallback(Rml::DataModelConstructor& constructor,
                                            std::string_view name,
                                            Owner* owner,
                                            IndexedEventHandler<Owner> handler) {
    if (!owner || !handler) {
        return false;
    }

    // 将 RmlUi 的原生回调参数：
    //   (DataModelHandle, event, arguments)
    // 适配为项目约定的成员函数调用：
    //   handler(slot_index, event)
    return constructor.BindEventCallback(
        Rml::String{name.data(), name.size()},
        [owner, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
            (owner->*handler)(getSingleIntArgument(arguments), event);
        });
}

// 绑定单个“上下文 + 槽位索引”的事件回调。
template<typename Owner, typename Context>
[[nodiscard]] bool bindIndexedContextEventCallback(Rml::DataModelConstructor& constructor,
                                                   std::string_view name,
                                                   Owner* owner,
                                                   Context context,
                                                   IndexedContextEventHandler<Owner, Context> handler) {
    if (!owner || !handler) {
        return false;
    }

    return constructor.BindEventCallback(
        Rml::String{name.data(), name.size()},
        [owner, context, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
            (owner->*handler)(context, getSingleIntArgument(arguments), event);
        });
}

// 绑定一组 IndexedEventBinding。
// 采用“跳过空 handler + 失败即返回”的策略，方便调用方统一判错。
template<typename Owner>
[[nodiscard]] bool bindIndexedEventCallbacks(Rml::DataModelConstructor& constructor,
                                             Owner* owner,
                                             std::initializer_list<IndexedEventBinding<Owner>> bindings) {
    // 批量注册回调，任意一项失败则立即返回 false。
    for (const auto& binding : bindings) {
        if (!binding.handler) {
            continue;
        }
        if (!bindIndexedEventCallback(constructor, binding.name, owner, binding.handler)) {
            return false;
        }
    }
    return true;
}

// 按统一命名规则批量绑定槽位网格事件。
// 事件名由 prefix + 后缀组成，例如 "slot" + "_mouse_up" -> "slot_mouse_up"。
// 这要求 RML 中 `data-event-*` 的函数名与这里拼出的名字保持一致。
template<typename Owner>
[[nodiscard]] bool bindSlotGridEvents(Rml::DataModelConstructor& constructor,
                                      std::string_view prefix,
                                      Owner* owner,
                                      const SlotGridEventHandlers<Owner>& handlers) {
    // 将前缀与后缀拼为完整事件名，例如 "inventory" + "_drag_start" → "inventory_drag_start"。
    // 这里按值捕获 prefix，确保 lambda 内持有稳定副本，不依赖外部参数生命周期。
    const auto make_name = [prefix](std::string_view suffix) {
        std::string name{prefix};
        name += suffix;
        return name;
    };

    // 1) 空处理函数过滤；2) 事件名拼接；3) 调用统一注册函数。
    const auto bind = [&](std::string_view suffix, IndexedEventHandler<Owner> handler) {
        if (!handler) {
            return true;
        }
        const std::string event_name = make_name(suffix);
        return bindIndexedEventCallback(constructor, event_name, owner, handler);
    };

    // 依次绑定常见交互事件，任意一项失败都返回 false。
    return bind("_focus", handlers.on_focus) &&
           bind("_mouse_down", handlers.on_mouse_down) &&
           bind("_mouse_up", handlers.on_mouse_up) &&
           bind("_hover_enter", handlers.on_hover_enter) &&
           bind("_hover_exit", handlers.on_hover_exit) &&
           bind("_drag_start", handlers.on_drag_start) &&
           bind("_drag_drop", handlers.on_drag_drop) &&
           bind("_drag_end", handlers.on_drag_end);
}

// 按统一命名规则批量绑定带额外上下文的槽位网格事件。
template<typename Owner, typename Context>
[[nodiscard]] bool bindSlotGridContextEvents(Rml::DataModelConstructor& constructor,
                                             std::string_view prefix,
                                             Owner* owner,
                                             Context context,
                                             const SlotGridContextEventHandlers<Owner, Context>& handlers) {
    const auto make_name = [prefix](std::string_view suffix) {
        std::string name{prefix};
        name += suffix;
        return name;
    };

    const auto bind = [&](std::string_view suffix, IndexedContextEventHandler<Owner, Context> handler) {
        if (!handler) {
            return true;
        }
        const std::string event_name = make_name(suffix);
        return bindIndexedContextEventCallback(constructor, event_name, owner, context, handler);
    };

    return bind("_focus", handlers.on_focus) &&
           bind("_mouse_down", handlers.on_mouse_down) &&
           bind("_mouse_up", handlers.on_mouse_up) &&
           bind("_hover_enter", handlers.on_hover_enter) &&
           bind("_hover_exit", handlers.on_hover_exit) &&
           bind("_drag_start", handlers.on_drag_start) &&
           bind("_drag_drop", handlers.on_drag_drop) &&
           bind("_drag_end", handlers.on_drag_end);
}

} // namespace game::ui
