#pragma once

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
    void clear();
    void start();

    bool active{false};
    bool drop_handled{false};
};

// 向 RmlUi 注册 SlotGridViewModel 的字段类型，使其可被数据绑定读取。
[[nodiscard]] bool registerSlotGridViewModelType(Rml::DataModelConstructor& constructor);

// 根据可选物品数据与 UI 选项，填充单个槽位的视图模型。
void populateSlotGridViewModel(SlotGridViewModel& view_model,
                               const std::optional<engine::ui::SlotItem>& item,
                               game::data::ItemCatalog* item_catalog,
                               const SlotGridViewModelOptions& options = {});

// 从 RmlUi 回调的 arguments 中提取单个整数参数（槽位索引）。
[[nodiscard]] int getSingleIntArgument(const Rml::VariantList& arguments);
// 从事件参数中解析拖拽来源信息，不存在时返回 nullopt。
[[nodiscard]] std::optional<SlotGridDragInfo> getSlotGridDragInfo(const Rml::Event& event);

// 统一的“按槽位索引分发”成员函数签名。
// 约定参数顺序为 (slot_index, event)。
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

// 绑定单个“带槽位索引”的事件回调。
// 若 owner 或 handler 为空，直接返回 false，避免悬空调用。
template<typename Owner>
[[nodiscard]] bool bindIndexedEventCallback(Rml::DataModelConstructor& constructor,
                                            std::string_view name,
                                            Owner* owner,
                                            IndexedEventHandler<Owner> handler) {
    if (!owner || !handler) {
        return false;
    }

    // 将 RmlUi 的 (event, arguments) 适配为强类型成员函数调用：
    // handler(slot_index, event)。
    return constructor.BindEventCallback(
        Rml::String{name.data(), name.size()},
        [owner, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
            (owner->*handler)(getSingleIntArgument(arguments), event);
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
// 事件名由 prefix + 后缀组成，例如 "inventory" + "_mouse_down"。
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

} // namespace game::ui
