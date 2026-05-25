// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/scene/battle_menu_model.h"

namespace game::scene {
namespace {

TEST(BattleMenuModelTest, SyncSelectionFlagsTracksCurrentCursors) {
    BattleMenuModel model;
    model.party_commands = {
        BattleCommandViewModel{.entry_index = 0},
        BattleCommandViewModel{.entry_index = 1},
    };
    model.actor_commands = {
        BattleCommandViewModel{.entry_index = 0},
        BattleCommandViewModel{.entry_index = 1},
        BattleCommandViewModel{.entry_index = 2},
    };
    model.list_entries = {
        BattleListEntryViewModel{.entry_index = 0},
        BattleListEntryViewModel{.entry_index = 1},
    };
    model.target_entries = {
        BattleTargetEntryViewModel{.entry_index = 3},
        BattleTargetEntryViewModel{.entry_index = 4},
    };

    model.party_command_cursor = 1;
    model.actor_command_cursor = 2;
    model.list_entry_cursor = 0;
    model.target_entry_cursor = 4;
    model.syncSelectionFlags();

    EXPECT_FALSE(model.party_commands[0].selected);
    EXPECT_TRUE(model.party_commands[1].selected);
    EXPECT_FALSE(model.actor_commands[0].selected);
    EXPECT_FALSE(model.actor_commands[1].selected);
    EXPECT_TRUE(model.actor_commands[2].selected);
    EXPECT_TRUE(model.list_entries[0].selected);
    EXPECT_FALSE(model.list_entries[1].selected);
    EXPECT_FALSE(model.target_entries[0].selected);
    EXPECT_TRUE(model.target_entries[1].selected);
}

} // namespace
} // namespace game::scene
// NOLINTEND
