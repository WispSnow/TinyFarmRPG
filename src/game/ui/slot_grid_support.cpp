#include "game/ui/slot_grid_support.h"

#include "game/data/item_catalog.h"
#include "game/ui/rml_item_icon_helpers.h"

#include <RmlUi/Core/DataTypeRegister.h>

#include <string>

namespace game::ui {

void SlotGridDragState::clear() {
    active = false;
    drop_handled = false;
    suppress_next_primary_mouse_up = false;
    source = SourceKind::None;
    source_slot_index = -1;
    source_inventory_slot_index = -1;
}

void SlotGridDragState::startFromInventory(int slot_index) {
    active = true;
    drop_handled = false;
    suppress_next_primary_mouse_up = true;
    source = SourceKind::Inventory;
    source_slot_index = slot_index;
    source_inventory_slot_index = slot_index;
}

void SlotGridDragState::startFromHotbar(int slot_index, int inventory_slot_index) {
    active = true;
    drop_handled = false;
    suppress_next_primary_mouse_up = true;
    source = SourceKind::Hotbar;
    source_slot_index = slot_index;
    source_inventory_slot_index = inventory_slot_index;
}

bool registerSlotGridViewModelType(Rml::DataModelConstructor& constructor) {
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
    view_model.icon_decorator = std::string{game::ui::kNoDecorator};
    view_model.count_text.clear();
    view_model.has_item = false;
    view_model.has_count = false;
    view_model.can_drag = false;
    view_model.is_selected = options.is_selected;
    view_model.is_active = options.is_active;
    view_model.label = Rml::String{options.label.data(), options.label.size()};

    if (!item || item->item_id == entt::null || item->count <= 0) {
        return;
    }

    view_model.icon_decorator = buildItemIconDecorator(item_catalog, item->item_id);
    view_model.has_item = hasDecorator(view_model.icon_decorator);
    view_model.can_drag = options.can_drag && view_model.has_item;

    if (item->count > 1 && view_model.has_item) {
        view_model.count_text = std::to_string(item->count);
        view_model.has_count = true;
    }
}

int getSingleIntArgument(const Rml::VariantList& arguments) {
    return (arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1);
}

} // namespace game::ui
