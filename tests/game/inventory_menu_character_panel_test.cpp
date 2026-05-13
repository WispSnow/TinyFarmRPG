// NOLINTBEGIN
#include <gtest/gtest.h>

#include <entt/entity/registry.hpp>

#include "game/component/party_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/rpg_catalog.h"
#include "game/scene/inventory_menu_character_panel.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace game::scene {
namespace {

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

[[nodiscard]] game::data::RpgCatalog loadProjectPartyCatalog() {
    game::data::RpgCatalog catalog;
    const auto rpg_root = (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/rpg").lexically_normal();
    EXPECT_TRUE(catalog.loadClasses((rpg_root / "classes.json").string()));
    EXPECT_TRUE(catalog.loadActors((rpg_root / "actors.json").string()));
    return catalog;
}

void setParty(entt::registry& registry, entt::entity player, std::vector<std::string> active_actor_ids) {
    registry.emplace_or_replace<game::component::PartyComponent>(
        player,
        game::component::PartyComponent{
            .recruited_actor_ids_ = {"actor.player", "actor.lyria", "actor.tori"},
            .active_actor_ids_ = std::move(active_actor_ids),
            .max_active_members_ = 4,
        });
}

[[nodiscard]] int filledMemberCount(const InventoryMenuPartyPanelData& data) {
    return static_cast<int>(std::count_if(data.party_members.begin(), data.party_members.end(), [](const auto& member) {
        return !member.empty;
    }));
}

TEST(InventoryMenuPartyPanelTest, UsesWalletGoldAndCatalogActorData) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerWalletComponent>(
        player,
        game::component::PlayerWalletComponent{.gold_ = 345});

    const game::data::RpgCatalog catalog = loadProjectPartyCatalog();
    const InventoryMenuPartyPanelData data =
        buildInventoryMenuPartyPanelData(registry, player, &catalog, "", false);

    ASSERT_EQ(data.party_members.size(), 4U);
    EXPECT_EQ(data.party_members[0].actor_id, "actor.player");
    EXPECT_EQ(data.party_members[0].display_name, "Alex");
    EXPECT_EQ(data.party_members[0].class_label, "剑客");
    EXPECT_EQ(data.party_members[0].hp_text, "HP 544/544");
    EXPECT_EQ(data.party_members[0].portrait_decorator, "image(portrait-default)");
    EXPECT_TRUE(data.party_members[0].selected);
    EXPECT_EQ(filledMemberCount(data), 1);
    EXPECT_EQ(data.gold_label, "Gold: 345");
    EXPECT_EQ(data.farm_label, "TinyFarm");
}

TEST(InventoryMenuPartyPanelTest, BuildsStableFourCardSnapshotsForPartySizes) {
    const game::data::RpgCatalog catalog = loadProjectPartyCatalog();
    const std::vector<std::string> actors = {"actor.player", "actor.lyria", "actor.tori", "actor.player"};

    for (int size = 1; size <= 4; ++size) {
        entt::registry registry;
        const entt::entity player = registry.create();
        setParty(registry, player, std::vector<std::string>(actors.begin(), actors.begin() + size));

        const InventoryMenuPartyPanelData data =
            buildInventoryMenuPartyPanelData(registry, player, &catalog, "actor.lyria", true);

        ASSERT_EQ(data.party_members.size(), 4U);
        EXPECT_EQ(filledMemberCount(data), size);
        for (int i = 0; i < size; ++i) {
            EXPECT_TRUE(data.party_members[static_cast<std::size_t>(i)].targetable);
        }
        for (int i = size; i < 4; ++i) {
            EXPECT_TRUE(data.party_members[static_cast<std::size_t>(i)].empty);
            EXPECT_FALSE(data.party_members[static_cast<std::size_t>(i)].targetable);
        }
    }
}

TEST(InventoryMenuPartyPanelTest, UsesRuntimeHpMpWhenPresent) {
    entt::registry registry;
    const entt::entity player = registry.create();
    setParty(registry, player, {"actor.player"});
    registry.emplace<game::component::PartyRuntimeStatsComponent>(
        player,
        game::component::PartyRuntimeStatsComponent{
            .states_by_actor_id_ = {{"actor.player", {.current_hp = 321, .current_mp = 12}}},
        });

    const game::data::RpgCatalog catalog = loadProjectPartyCatalog();
    const InventoryMenuPartyPanelData data =
        buildInventoryMenuPartyPanelData(registry, player, &catalog, "actor.player", false);

    ASSERT_EQ(data.party_members.size(), 4U);
    EXPECT_EQ(data.party_members[0].hp_text, "HP 321/544");
    EXPECT_EQ(data.party_members[0].mp_text, "MP 12/41");
}

} // namespace
} // namespace game::scene
// NOLINTEND
