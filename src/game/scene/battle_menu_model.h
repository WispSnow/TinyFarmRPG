#pragma once

#include "game/scene/battle_scene_state.h"
#include "game/scene/battle_scene_view_models.h"

#include <RmlUi/Core/Types.h>

#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
}

namespace engine::ui::rmlui {
class RmlDocumentController;
}

namespace game::scene {

/// @brief BattleScene 的菜单相关 RmlUi 数据模型状态。
class BattleMenuModel final {
public:
    BattleMenuState state{BattleMenuState::None};
    bool focus_dirty{true};

    Rml::String turn_text{"Turn: -"};
    Rml::String result_text{"Result: Choose action"};
    bool actions_enabled{false};
    std::string status_text{"Choose action"};
    Rml::String list_empty_text{"No entries available"};
    Rml::String target_empty_text{"No targets available"};
    bool party_command_visible{false};
    bool actor_command_visible{false};
    bool list_menu_visible{false};
    bool target_menu_visible{false};
    bool list_empty{true};
    bool target_empty{true};

    std::vector<BattleCommandViewModel> party_commands{};
    std::vector<BattleCommandViewModel> actor_commands{};
    std::vector<BattleListEntryViewModel> list_entries{};
    std::vector<BattleTargetEntryViewModel> target_entries{};
    int party_command_cursor{0};
    int actor_command_cursor{0};
    int list_entry_cursor{-1};
    int target_entry_cursor{-1};

    [[nodiscard]] bool bind(Rml::DataModelConstructor& constructor);
    [[nodiscard]] bool registerArrays(Rml::DataModelConstructor& constructor);

    void markDirty(engine::ui::rmlui::RmlDocumentController& document_controller) const;
    void markActiveSelectionDirty(engine::ui::rmlui::RmlDocumentController& document_controller) const;
    void setState(BattleMenuState next_state, engine::ui::rmlui::RmlDocumentController& document_controller);
    void setHint(std::string_view text, engine::ui::rmlui::RmlDocumentController& document_controller);
    void refreshCommandEnabled(bool enabled, engine::ui::rmlui::RmlDocumentController& document_controller);
    void syncSelectionFlags();
};

} // namespace game::scene
