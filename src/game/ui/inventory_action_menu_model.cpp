#include "game/ui/inventory_action_menu_model.h"

#include <RmlUi/Core/DataTypeRegister.h>

#include <utility>

namespace game::ui {

bool InventoryActionMenuModel::bind(Rml::DataModelConstructor& constructor) {
    if (auto action_handle = constructor.RegisterStruct<InventoryActionEntryViewModel>()) {
        action_handle.RegisterMember("action_id", &InventoryActionEntryViewModel::action_id);
        action_handle.RegisterMember("label", &InventoryActionEntryViewModel::label);
        action_handle.RegisterMember("is_destructive", &InventoryActionEntryViewModel::is_destructive);
    } else {
        return false;
    }

    return constructor.RegisterArray<decltype(entries)>() &&
           constructor.Bind("action_menu_entries", &entries) &&
           constructor.Bind("action_menu_title", &title) &&
           constructor.Bind("action_menu_visible", &visible);
}

void InventoryActionMenuModel::show(Rml::String next_title,
                                    std::vector<InventoryActionEntryViewModel> next_entries,
                                    engine::ui::rmlui::RmlDocumentController& document_controller) {
    entries = std::move(next_entries);
    title = std::move(next_title);
    visible = true;
    markDirty(document_controller);
}

void InventoryActionMenuModel::close(engine::ui::rmlui::RmlDocumentController& document_controller) {
    if (!visible) {
        return;
    }

    // Hide the data-if subtree first. Clearing entries in the same UI tick can
    // leave stale data-for instances querying entries[n] during RmlUi refresh.
    visible = false;
    markDirty(document_controller);
}

void InventoryActionMenuModel::markDirty(engine::ui::rmlui::RmlDocumentController& document_controller) const {
    if (!document_controller.isModelValid()) {
        return;
    }

    document_controller.markDirty("action_menu_entries");
    document_controller.markDirty("action_menu_title");
    document_controller.markDirty("action_menu_visible");
}

} // namespace game::ui
