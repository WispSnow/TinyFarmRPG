#include "engine/input/input_context_registry.h"

#include <entt/core/hashed_string.hpp>

namespace engine::input {

InputMappingTable defaultInputMappings() {
    return {
        {"move_left", {"A", "Left", "GamepadDpadLeft", "LeftStickLeft"}},
        {"move_right", {"D", "Right", "GamepadDpadRight", "LeftStickRight"}},
        {"move_up", {"W", "Up", "GamepadDpadUp", "LeftStickUp"}},
        {"move_down", {"S", "Down", "GamepadDpadDown", "LeftStickDown"}},
        {"menu_left", {"A", "Left", "GamepadDpadLeft", "LeftStickLeft"}},
        {"menu_right", {"D", "Right", "GamepadDpadRight", "LeftStickRight"}},
        {"menu_up", {"W", "Up", "GamepadDpadUp", "LeftStickUp"}},
        {"menu_down", {"S", "Down", "GamepadDpadDown", "LeftStickDown"}},
        {"menu_confirm", {"Return", "Space", "GamepadSouth"}},
        {"menu_cancel", {"Escape", "GamepadEast"}},
        {"primary_action", {"MouseLeft", "GamepadSouth"}},
        {"secondary_action", {"MouseRight", "GamepadEast"}},
        {"pause", {"P", "Escape", "GamepadStart"}},
        {"interact", {"F", "GamepadWest"}},
        {"inventory", {"I", "GamepadBack"}},
        {"inventory_tab_equipment", {"C"}},
        {"inventory_tab_quests", {"J"}},
        {"inventory_tab_map", {"M"}},
        {"inventory_tab_options", {"O"}},
        {"toggle_prompt_bar", {"F1"}},
        {"hotbar", {"Tab", "GamepadNorth"}},
        {"hotbar_prev", {"GamepadLeftShoulder"}},
        {"hotbar_next", {"GamepadRightShoulder"}},
        {"rotate_left", {"Q"}},
        {"rotate_right", {"E"}},
        {"player_light", {"L"}},
        {"camera_reset_zoom", {"MouseMiddle"}},
        {"hotbar_1", {"1"}},
        {"hotbar_2", {"2"}},
        {"hotbar_3", {"3"}},
        {"hotbar_4", {"4"}},
        {"hotbar_5", {"5"}},
        {"hotbar_6", {"6"}},
        {"hotbar_7", {"7"}},
        {"hotbar_8", {"8"}},
        {"hotbar_9", {"9"}},
        {"hotbar_10", {"0"}},
    };
}

bool isMenuNavigationActionName(const std::string_view action_name) {
    return action_name == "menu_up"
        || action_name == "menu_down"
        || action_name == "menu_left"
        || action_name == "menu_right"
        || action_name == "menu_confirm"
        || action_name == "menu_cancel";
}

bool isMenuLikeContext(const std::optional<InputContextId> context_id) {
    return context_id == InputContextId::Menu
        || context_id == InputContextId::Dialogue
        || context_id == InputContextId::Battle;
}

std::unordered_map<InputContextId, InputContextDefinition> buildInputContextDefinitions(
    const std::vector<entt::id_type>& action_dispatch_order,
    const std::unordered_map<entt::id_type, ActionEntry>& actions) {
    std::unordered_map<InputContextId, InputContextDefinition> definitions;

    const auto build_definition = [&](std::initializer_list<const char*> action_names) {
        InputContextDefinition definition;

        for (const char* action_name : action_names) {
            const auto action_id = entt::hashed_string{action_name}.value();
            if (actions.contains(action_id)) {
                definition.allowed_actions.insert(action_id);
            }
        }

        definition.dispatch_actions.reserve(definition.allowed_actions.size());
        for (auto action_id : action_dispatch_order) {
            if (definition.allowed_actions.contains(action_id)) {
                definition.dispatch_actions.push_back(action_id);
            }
        }

        return definition;
    };

    definitions.emplace(
        InputContextId::Gameplay,
        build_definition({
            "move_left",
            "move_right",
            "move_up",
            "move_down",
            "primary_action",
            "secondary_action",
            "interact",
            "pause",
            "inventory",
            "inventory_tab_equipment",
            "inventory_tab_quests",
            "inventory_tab_map",
            "inventory_tab_options",
            "toggle_prompt_bar",
            "hotbar",
            "hotbar_prev",
            "hotbar_next",
            "hotbar_1",
            "hotbar_2",
            "hotbar_3",
            "hotbar_4",
            "hotbar_5",
            "hotbar_6",
            "hotbar_7",
            "hotbar_8",
            "hotbar_9",
            "hotbar_10",
            "rotate_left",
            "rotate_right",
            "player_light",
            "camera_reset_zoom",
        }));
    definitions.emplace(
        InputContextId::Menu,
        build_definition({
            "menu_left",
            "menu_right",
            "menu_up",
            "menu_down",
            "menu_confirm",
            "menu_cancel",
            "inventory",
            "inventory_tab_equipment",
            "inventory_tab_quests",
            "inventory_tab_map",
            "inventory_tab_options",
        }));
    definitions.emplace(
        InputContextId::Dialogue,
        build_definition({"menu_left", "menu_right", "menu_up", "menu_down", "menu_confirm", "menu_cancel"}));
    definitions.emplace(
        InputContextId::Battle,
        build_definition({"menu_left", "menu_right", "menu_up", "menu_down", "menu_confirm", "menu_cancel"}));

    return definitions;
}

} // namespace engine::input
