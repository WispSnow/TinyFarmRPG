// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/component/party_component.h"
#include "game/component/party_equipment_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_types.h"
#include "game/defs/events.h"
#include "game/domain/party_rest_service.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace game::domain {
namespace {

struct FixturePaths {
    std::filesystem::path classes{};
    std::filesystem::path actors{};
    std::filesystem::path equipment{};
};

[[nodiscard]] FixturePaths createFixture() {
    const auto temp_root = game::test::createUniqueTempDir("party_rest_service");
    const auto data_root = temp_root / "rpg";
    std::filesystem::create_directories(data_root);

    game::test::writeTextFile(
        data_root / "classes.json",
        R"json({
  "classes": [
    { "id": "class.hero", "display_name": "Hero", "base_params": [100, 20, 10, 10, 10, 10, 10, 10] },
    { "id": "class.mage", "display_name": "Mage", "base_params": [80, 50, 8, 8, 12, 10, 10, 10] }
  ]
})json");

    game::test::writeTextFile(
        data_root / "actors.json",
        R"json({
  "actors": [
    { "id": "actor.player", "display_name": "Alex", "class_id": "class.hero", "initial_level": 1, "max_level": 99 },
    { "id": "actor.lyria", "display_name": "Lyria", "class_id": "class.mage", "initial_level": 1, "max_level": 99 },
    { "id": "actor.tori", "display_name": "Tori", "class_id": "class.hero", "initial_level": 1, "max_level": 99 }
  ]
})json");

    game::test::writeTextFile(
        data_root / "equipment.json",
        R"json({
  "equipment": [
    { "item_id": "equip.vital_charm", "slot": "accessory", "param_bonuses": { "mhp": 50, "mmp": 10 } }
  ]
})json");

    return FixturePaths{
        .classes = data_root / "classes.json",
        .actors = data_root / "actors.json",
        .equipment = data_root / "equipment.json",
    };
}

[[nodiscard]] game::data::RpgCatalog loadCatalog() {
    const auto paths = createFixture();
    game::data::RpgCatalog catalog{};
    EXPECT_TRUE(catalog.loadClasses(paths.classes.string()));
    EXPECT_TRUE(catalog.loadActors(paths.actors.string()));
    EXPECT_TRUE(catalog.loadEquipment(paths.equipment.string()));
    return catalog;
}

[[nodiscard]] entt::entity createPlayerWithParty(entt::registry& registry,
                                                 std::vector<std::string> recruited,
                                                 std::vector<std::string> active) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::PartyComponent>(
        player,
        game::component::PartyComponent{
            .recruited_actor_ids_ = std::move(recruited),
            .active_actor_ids_ = std::move(active),
            .max_active_members_ = 4,
        });
    return player;
}

struct RuntimeStatsChangedCapture {
    int count{0};
    entt::entity player{entt::null};
    bool full_sync{false};

    void onChanged(const game::defs::PartyRuntimeStatsChanged& evt) {
        ++count;
        player = evt.player;
        full_sync = evt.full_sync;
    }
};

} // namespace

TEST(PartyRestServiceTest, ThreeHoursRecoversThirtyPercentAndPreviewMatchesApply) {
    const auto catalog = loadCatalog();
    entt::registry registry{};
    const entt::entity player = createPlayerWithParty(registry, {"actor.player"}, {"actor.player"});
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 20,
        .current_mp = 4,
        .level = 1,
        .total_exp = 0,
    };

    const auto preview = PartyRestService::previewActivePartyRecovery(registry, player, catalog, 3);
    ASSERT_EQ(preview.members.size(), 1U);
    EXPECT_EQ(preview.recovery_percent, 30);
    EXPECT_EQ(preview.members[0].after_hp, 50);
    EXPECT_EQ(preview.members[0].after_mp, 10);

    const auto result = PartyRestService::applyActivePartyRecovery(registry, player, catalog, 3);
    EXPECT_TRUE(result.runtime_state_changed);
    EXPECT_EQ(result.preview.members[0].after_hp, preview.members[0].after_hp);
    EXPECT_EQ(result.preview.members[0].after_mp, preview.members[0].after_mp);

    const auto& stored = runtime.states_by_actor_id_.at("actor.player");
    EXPECT_EQ(stored.current_hp, preview.members[0].after_hp);
    EXPECT_EQ(stored.current_mp, preview.members[0].after_mp);
    EXPECT_EQ(runtime.revision_, 1U);
}

TEST(PartyRestServiceTest, TenHoursFullyRecoversAndRevivesZeroHpActor) {
    const auto catalog = loadCatalog();
    entt::registry registry{};
    const entt::entity player = createPlayerWithParty(registry, {"actor.player"}, {"actor.player"});
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 0,
        .current_mp = 0,
        .level = 1,
        .total_exp = 0,
    };

    const auto result = PartyRestService::applyActivePartyRecovery(registry, player, catalog, 10);

    EXPECT_TRUE(result.preview.full_recovery);
    EXPECT_TRUE(result.runtime_state_changed);
    const auto& stored = runtime.states_by_actor_id_.at("actor.player");
    EXPECT_EQ(stored.current_hp, 100);
    EXPECT_EQ(stored.current_mp, 20);
}

TEST(PartyRestServiceTest, RecoversOnlyActivePartyMembers) {
    const auto catalog = loadCatalog();
    entt::registry registry{};
    const entt::entity player =
        createPlayerWithParty(registry, {"actor.player", "actor.lyria"}, {"actor.player"});
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 10,
        .current_mp = 1,
        .level = 1,
        .total_exp = 0,
    };
    runtime.states_by_actor_id_["actor.lyria"] = game::component::ActorRuntimeState{
        .current_hp = 10,
        .current_mp = 1,
        .level = 1,
        .total_exp = 0,
    };

    const auto result = PartyRestService::applyActivePartyRecovery(registry, player, catalog, 3);

    ASSERT_EQ(result.preview.members.size(), 1U);
    EXPECT_EQ(result.preview.members[0].actor_id, "actor.player");
    EXPECT_EQ(runtime.states_by_actor_id_.at("actor.player").current_hp, 40);
    EXPECT_EQ(runtime.states_by_actor_id_.at("actor.player").current_mp, 7);
    EXPECT_EQ(runtime.states_by_actor_id_.at("actor.lyria").current_hp, 10);
    EXPECT_EQ(runtime.states_by_actor_id_.at("actor.lyria").current_mp, 1);
}

TEST(PartyRestServiceTest, EmptyActivePartyIsNoopWhenPartyComponentExists) {
    const auto catalog = loadCatalog();
    entt::registry registry{};
    const entt::entity player = createPlayerWithParty(registry, {"actor.player"}, {});
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.revision_ = 7;
    runtime.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 1,
        .current_mp = 1,
        .level = 1,
        .total_exp = 0,
    };

    const auto preview = PartyRestService::previewActivePartyRecovery(registry, player, catalog, 3);
    const auto result = PartyRestService::applyActivePartyRecovery(registry, player, catalog, 3);

    EXPECT_TRUE(preview.empty());
    EXPECT_FALSE(result.runtime_state_changed);
    EXPECT_EQ(runtime.revision_, 7U);
    EXPECT_EQ(runtime.states_by_actor_id_.at("actor.player").current_hp, 1);
}

TEST(PartyRestServiceTest, MissingPartyComponentFallsBackToDefaultPlayerActor) {
    const auto catalog = loadCatalog();
    entt::registry registry{};
    const entt::entity player = registry.create();
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 10,
        .current_mp = 1,
        .level = 1,
        .total_exp = 0,
    };

    const auto result = PartyRestService::applyActivePartyRecovery(registry, player, catalog, 1);

    ASSERT_EQ(result.preview.members.size(), 1U);
    EXPECT_EQ(result.preview.members[0].actor_id, "actor.player");
    EXPECT_EQ(runtime.states_by_actor_id_.at("actor.player").current_hp, 20);
    EXPECT_EQ(runtime.states_by_actor_id_.at("actor.player").current_mp, 3);
}

TEST(PartyRestServiceTest, EquipmentBonusesAffectRecoveryAmount) {
    const auto catalog = loadCatalog();
    entt::registry registry{};
    const entt::entity player = createPlayerWithParty(registry, {"actor.player"}, {"actor.player"});
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 10,
        .current_mp = 1,
        .level = 1,
        .total_exp = 0,
    };
    auto& equipment = registry.emplace<game::component::PartyEquipmentComponent>(player);
    equipment.loadouts_by_actor_id_["actor.player"].equipped_item_ids_[game::data::EquipmentSlotId::Accessory] =
        game::data::RpgCatalog::hashId("equip.vital_charm");

    const auto preview = PartyRestService::previewActivePartyRecovery(registry, player, catalog, 2);

    ASSERT_EQ(preview.members.size(), 1U);
    EXPECT_EQ(preview.members[0].max_hp, 150);
    EXPECT_EQ(preview.members[0].max_mp, 30);
    EXPECT_EQ(preview.members[0].after_hp, 40);
    EXPECT_EQ(preview.members[0].after_mp, 7);
}

TEST(PartyRestServiceTest, RevisionAndCallerEventAreOncePerApply) {
    const auto catalog = loadCatalog();
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    RuntimeStatsChangedCapture capture{};
    dispatcher.sink<game::defs::PartyRuntimeStatsChanged>()
        .connect<&RuntimeStatsChangedCapture::onChanged>(&capture);

    const entt::entity player =
        createPlayerWithParty(registry, {"actor.player", "actor.lyria"}, {"actor.player", "actor.lyria"});
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.revision_ = 5;
    runtime.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 1,
        .current_mp = 1,
        .level = 1,
        .total_exp = 0,
    };
    runtime.states_by_actor_id_["actor.lyria"] = game::component::ActorRuntimeState{
        .current_hp = 1,
        .current_mp = 1,
        .level = 1,
        .total_exp = 0,
    };

    const auto result = PartyRestService::applyActivePartyRecovery(registry, player, catalog, 3);
    if (result.runtime_state_changed) {
        dispatcher.trigger(game::defs::PartyRuntimeStatsChanged{
            .player = player,
            .actor_id = {},
            .full_sync = true,
        });
    }

    EXPECT_TRUE(result.runtime_state_changed);
    EXPECT_EQ(runtime.revision_, 6U);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.player, player);
    EXPECT_TRUE(capture.full_sync);
}

} // namespace game::domain
// NOLINTEND
