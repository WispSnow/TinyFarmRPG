#include "game/scene/battle_menu_model.h"

#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <algorithm>

namespace game::scene {

bool BattleMenuModel::bind(Rml::DataModelConstructor& constructor) {
    return constructor.Bind("turn_text", &turn_text) &&
        constructor.Bind("result_text", &result_text) &&
        constructor.Bind("actions_enabled", &actions_enabled) &&
        constructor.Bind("list_empty_text", &list_empty_text) &&
        constructor.Bind("target_empty_text", &target_empty_text) &&
        constructor.Bind("party_command_visible", &party_command_visible) &&
        constructor.Bind("actor_command_visible", &actor_command_visible) &&
        constructor.Bind("list_menu_visible", &list_menu_visible) &&
        constructor.Bind("target_menu_visible", &target_menu_visible) &&
        constructor.Bind("list_empty", &list_empty) &&
        constructor.Bind("target_empty", &target_empty) &&
        constructor.Bind("party_commands", &party_commands) &&
        constructor.Bind("actor_commands", &actor_commands) &&
        constructor.Bind("list_entries", &list_entries) &&
        constructor.Bind("target_entries", &target_entries);
}

bool BattleMenuModel::registerArrays(Rml::DataModelConstructor& constructor) {
    return constructor.RegisterArray<decltype(party_commands)>() &&
        constructor.RegisterArray<decltype(list_entries)>() &&
        constructor.RegisterArray<decltype(target_entries)>();
}

void BattleMenuModel::markDirty(engine::ui::rmlui::RmlDocumentController& document_controller) const {
    document_controller.markDirty("result_text");
    document_controller.markDirty("list_empty_text");
    document_controller.markDirty("target_empty_text");
    document_controller.markDirty("party_command_visible");
    document_controller.markDirty("actor_command_visible");
    document_controller.markDirty("list_menu_visible");
    document_controller.markDirty("target_menu_visible");
    document_controller.markDirty("list_empty");
    document_controller.markDirty("target_empty");
    document_controller.markDirty("party_commands");
    document_controller.markDirty("actor_commands");
    document_controller.markDirty("list_entries");
    document_controller.markDirty("target_entries");
}

void BattleMenuModel::setState(const BattleMenuState next_state,
                               engine::ui::rmlui::RmlDocumentController& document_controller) {
    state = next_state;
    party_command_visible = next_state == BattleMenuState::PartyCommand;
    actor_command_visible = next_state == BattleMenuState::ActorCommand;
    list_menu_visible = next_state == BattleMenuState::SkillList || next_state == BattleMenuState::ItemList;
    target_menu_visible = next_state == BattleMenuState::TargetSelect;
    list_empty = list_entries.empty();
    target_empty = target_entries.empty();

    switch (next_state) {
        case BattleMenuState::None:
            status_text = "Choose action";
            break;
        case BattleMenuState::PartyCommand:
            status_text = "Choose action";
            party_command_cursor = party_commands.empty()
                ? -1
                : std::clamp(party_command_cursor, 0, static_cast<int>(party_commands.size()) - 1);
            break;
        case BattleMenuState::ActorCommand:
            status_text = "Choose action";
            actor_command_cursor = actor_commands.empty()
                ? -1
                : std::clamp(actor_command_cursor, 0, static_cast<int>(actor_commands.size()) - 1);
            break;
        case BattleMenuState::SkillList:
            status_text = "Choose a skill";
            list_empty_text = "No skills available";
            list_entry_cursor = list_entries.empty() ? -1 : std::clamp(list_entry_cursor, 0, static_cast<int>(list_entries.size()) - 1);
            break;
        case BattleMenuState::ItemList:
            status_text = "Choose an item";
            list_empty_text = "No battle items available";
            list_entry_cursor = list_entries.empty() ? -1 : std::clamp(list_entry_cursor, 0, static_cast<int>(list_entries.size()) - 1);
            break;
        case BattleMenuState::TargetSelect:
            status_text = "Choose a target";
            target_empty_text = "No targets available";
            target_entry_cursor = target_entries.empty() ? -1 : std::clamp(target_entry_cursor, 0, static_cast<int>(target_entries.size()) - 1);
            break;
    }

    markDirty(document_controller);
    focus_dirty = true;
}

void BattleMenuModel::setHint(std::string_view text,
                              engine::ui::rmlui::RmlDocumentController& document_controller) {
    status_text = std::string{text};
    document_controller.markDirty("result_text");
}

void BattleMenuModel::refreshCommandEnabled(const bool enabled,
                                            engine::ui::rmlui::RmlDocumentController& document_controller) {
    bool changed = false;
    for (auto& command : party_commands) {
        if (command.enabled != enabled) {
            command.enabled = enabled;
            changed = true;
        }
    }
    for (auto& command : actor_commands) {
        if (command.enabled != enabled) {
            command.enabled = enabled;
            changed = true;
        }
    }

    if (changed) {
        document_controller.markDirty("party_commands");
        document_controller.markDirty("actor_commands");
        focus_dirty = true;
    }
}

} // namespace game::scene
