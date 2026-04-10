// NOLINTBEGIN
#include <gtest/gtest.h>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>

#include "engine/component/name_component.h"
#include "game/component/player_wallet_component.h"
#include "game/scene/inventory_menu_character_panel.h"

namespace game::scene {
namespace {

TEST(InventoryMenuCharacterPanelTest, UsesWalletGoldAndPlayerNameWhenAvailable) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<engine::component::NameComponent>(
        player,
        engine::component::NameComponent{
            .name_id_ = entt::hashed_string{"Avery"}.value(),
            .name_ = "Avery",
        });
    registry.emplace<game::component::PlayerWalletComponent>(
        player,
        game::component::PlayerWalletComponent{.gold_ = 345});

    const InventoryMenuCharacterPanelData data = buildInventoryMenuCharacterPanelData(registry, player);

    EXPECT_EQ(data.char_name, "Avery");
    EXPECT_EQ(data.char_title, "Lv.1 Farmer");
    EXPECT_EQ(data.gold_label, "Gold: 345");
    EXPECT_EQ(data.farm_label, "TinyFarm");
}

TEST(InventoryMenuCharacterPanelTest, FallsBackToZeroGoldWhenWalletMissing) {
    entt::registry registry;
    const entt::entity player = registry.create();

    const InventoryMenuCharacterPanelData data = buildInventoryMenuCharacterPanelData(registry, player);

    EXPECT_EQ(data.char_name, "Player");
    EXPECT_EQ(data.char_title, "Lv.1 Farmer");
    EXPECT_EQ(data.gold_label, "Gold: 0");
    EXPECT_EQ(data.farm_label, "TinyFarm");
}

} // namespace
} // namespace game::scene
// NOLINTEND
