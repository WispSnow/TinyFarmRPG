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

struct SlotGridViewModelOptions {
    bool can_drag{false};
    bool is_selected{false};
    bool is_active{false};
    std::string_view label{};
};

struct SlotGridDragState {
    enum class SourceKind {
        None,
        Inventory,
        Hotbar,
    };

    void clear();
    void startFromInventory(int slot_index);
    void startFromHotbar(int slot_index, int inventory_slot_index);

    [[nodiscard]] bool fromInventory() const { return source == SourceKind::Inventory; }
    [[nodiscard]] bool fromHotbar() const { return source == SourceKind::Hotbar; }

    bool active{false};
    bool drop_handled{false};
    bool suppress_next_primary_mouse_up{false};
    SourceKind source{SourceKind::None};
    int source_slot_index{-1};
    int source_inventory_slot_index{-1};
};

[[nodiscard]] bool registerSlotGridViewModelType(Rml::DataModelConstructor& constructor);

void populateSlotGridViewModel(SlotGridViewModel& view_model,
                               const std::optional<engine::ui::SlotItem>& item,
                               game::data::ItemCatalog* item_catalog,
                               const SlotGridViewModelOptions& options = {});

[[nodiscard]] int getSingleIntArgument(const Rml::VariantList& arguments);

template<typename Owner>
using IndexedEventHandler = void (Owner::*)(int, Rml::Event&);

template<typename Owner>
struct IndexedEventBinding {
    std::string_view name;
    IndexedEventHandler<Owner> handler{nullptr};
};

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

template<typename Owner>
[[nodiscard]] bool bindIndexedEventCallback(Rml::DataModelConstructor& constructor,
                                            std::string_view name,
                                            Owner* owner,
                                            IndexedEventHandler<Owner> handler) {
    if (!owner || !handler) {
        return false;
    }

    return constructor.BindEventCallback(
        Rml::String{name.data(), name.size()},
        [owner, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
            (owner->*handler)(getSingleIntArgument(arguments), event);
        });
}

template<typename Owner>
[[nodiscard]] bool bindIndexedEventCallbacks(Rml::DataModelConstructor& constructor,
                                             Owner* owner,
                                             std::initializer_list<IndexedEventBinding<Owner>> bindings) {
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

template<typename Owner>
[[nodiscard]] bool bindSlotGridEvents(Rml::DataModelConstructor& constructor,
                                      std::string_view prefix,
                                      Owner* owner,
                                      const SlotGridEventHandlers<Owner>& handlers) {
    const auto make_name = [prefix](std::string_view suffix) {
        std::string name{prefix};
        name += suffix;
        return name;
    };

    const auto bind = [&](std::string_view suffix, IndexedEventHandler<Owner> handler) {
        if (!handler) {
            return true;
        }
        const std::string event_name = make_name(suffix);
        return bindIndexedEventCallback(constructor, event_name, owner, handler);
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
