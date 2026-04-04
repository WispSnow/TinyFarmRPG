#include "game/ui/slot_grid_support.h"

#include "game/data/item_catalog.h"
#include "game/ui/rml_item_icon_helpers.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include <string>

namespace game::ui {

namespace {

// 拖拽元素上约定的属性键：用于跨事件传递来源与槽位索引。
constexpr char kSlotGridSourceAttribute[] = "slot_grid_source";
constexpr char kSlotGridIndexAttribute[] = "slot_index";

// 将整数值安全映射到枚举，非法值统一回退为 None。
[[nodiscard]] SlotGridDragSourceKind toSlotGridDragSourceKind(int value) {
    switch (value) {
        case static_cast<int>(SlotGridDragSourceKind::Inventory):
            return SlotGridDragSourceKind::Inventory;
        case static_cast<int>(SlotGridDragSourceKind::Hotbar):
            return SlotGridDragSourceKind::Hotbar;
        default:
            return SlotGridDragSourceKind::None;
    }
}

} // namespace

void SlotGridDragState::clear() {
    // 重置到“无拖拽进行中”的默认状态。
    active = false;
    drop_handled = false;
}

void SlotGridDragState::start() {
    // 开始一次新的拖拽流程，drop 标记需要重新计数。
    active = true;
    drop_handled = false;
}

bool registerSlotGridViewModelType(Rml::DataModelConstructor& constructor) {
    // 将 C++ 结构体字段暴露给 RmlUi，模板里可按成员名直接读取。
    if (auto slot_handle = constructor.RegisterStruct<SlotGridViewModel>()) {
        slot_handle.RegisterMember("slot_index", &SlotGridViewModel::slot_index);
        slot_handle.RegisterMember("icon_decorator", &SlotGridViewModel::icon_decorator);
        slot_handle.RegisterMember("count_text", &SlotGridViewModel::count_text);
        slot_handle.RegisterMember("label", &SlotGridViewModel::label);
        slot_handle.RegisterMember("has_item", &SlotGridViewModel::has_item);
        slot_handle.RegisterMember("has_count", &SlotGridViewModel::has_count);
        slot_handle.RegisterMember("can_drag", &SlotGridViewModel::can_drag);
        slot_handle.RegisterMember("is_selected", &SlotGridViewModel::is_selected);
        slot_handle.RegisterMember("is_active", &SlotGridViewModel::is_active);
        return true;
    }

    return false;
}

void populateSlotGridViewModel(SlotGridViewModel& view_model,
                               const std::optional<engine::ui::SlotItem>& item,
                               game::data::ItemCatalog* item_catalog,
                               const SlotGridViewModelOptions& options) {
    // 先写入统一默认值，避免复用对象时残留上一帧状态。
    view_model.icon_decorator = std::string{game::ui::kNoDecorator};
    view_model.count_text.clear();
    view_model.has_item = false;
    view_model.has_count = false;
    view_model.can_drag = false;
    view_model.is_selected = options.is_selected;
    view_model.is_active = options.is_active;
    view_model.label = Rml::String{options.label.data(), options.label.size()};

    // 空物品、非法 id 或数量不合法时，保持默认占位表现并提前返回。
    if (!item || item->item_id == entt::null || item->count <= 0) {
        return;
    }

    // 构建图标装饰串并据此决定是否“确实有可显示物品”。
    view_model.icon_decorator = buildItemIconDecorator(item_catalog, item->item_id);
    view_model.has_item = hasDecorator(view_model.icon_decorator);
    // 仅在配置允许且槽位有可见物品时可拖拽。
    view_model.can_drag = options.can_drag && view_model.has_item;

    // 数量大于 1 才显示叠堆数字，避免单件物品产生视觉噪音。
    if (item->count > 1 && view_model.has_item) {
        view_model.count_text = std::to_string(item->count);
        view_model.has_count = true;
    }
}

int getSingleIntArgument(const Rml::VariantList& arguments) {
    // 约定只接受一个整型参数；不满足约定时返回 -1。
    return (arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1);
}

std::optional<SlotGridDragInfo> getSlotGridDragInfo(const Rml::Event& event) {
    // RmlUi 拖拽事件通过 drag_element 传递来源元素指针。
    auto* drag_element = static_cast<Rml::Element*>(event.GetParameter<void*>("drag_element", nullptr));
    if (!drag_element) {
        return std::nullopt;
    }

    SlotGridDragInfo drag_info{};
    // 从拖拽元素属性读取来源与索引，缺省值用于兜底判错。
    drag_info.source =
        toSlotGridDragSourceKind(drag_element->GetAttribute<int>(kSlotGridSourceAttribute, 0));
    drag_info.slot_index = drag_element->GetAttribute<int>(kSlotGridIndexAttribute, -1);
    // 来源或索引无效时视为不可用拖拽数据。
    if (drag_info.source == SlotGridDragSourceKind::None || drag_info.slot_index < 0) {
        return std::nullopt;
    }

    return drag_info;
}

} // namespace game::ui
