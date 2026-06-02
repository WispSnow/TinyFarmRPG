// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/battle/actor_stats_resolver.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/player_identity_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/domain/actor_progression_service.h"

#include <entt/entity/registry.hpp>

#include <filesystem>
#include <limits>
#include <string>
#include <unordered_map>

namespace game::domain {
namespace {

struct Fixture {
    std::filesystem::path classes{};
    std::filesystem::path actors{};
};

[[nodiscard]] Fixture createFixture() {
    const auto temp_root = game::test::createUniqueTempDir("actor_progression_service");
    const auto data_root = temp_root / "rpg";
    std::filesystem::create_directories(data_root);

    game::test::writeTextFile(
        data_root / "classes.json",
        R"json({
  "classes": [
    {
      "id": "class.hero",
      "display_name": "Hero",
      "exp_curve": { "basis": 30, "extra": 20, "acc_a": 30, "acc_b": 30 },
      "base_params": [100, 20, 10, 10, 10, 10, 10, 10],
      "param_curves": {
        "mhp": { "level_1": 100, "level_99": 590, "shape": "linear" },
        "mmp": { "level_1": 20, "level_99": 118, "shape": "linear" },
        "atk": { "level_1": 10, "level_99": 108, "shape": "late" }
      }
    },
    {
      "id": "class.flat",
      "display_name": "Flat",
      "base_params": [80, 10, 8, 8, 8, 8, 8, 8]
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "actors.json",
        R"json({
  "actors": [
    { "id": "actor.player", "display_name": "Alex", "class_id": "class.hero", "initial_level": 1, "max_level": 99 },
    { "id": "actor.hero", "display_name": "Hero", "class_id": "class.hero", "initial_level": 1, "max_level": 99 },
    { "id": "actor.flat", "display_name": "Flat", "class_id": "class.flat", "initial_level": 1, "max_level": 99 }
  ]
})json");

    return Fixture{.classes = data_root / "classes.json", .actors = data_root / "actors.json"};
}

[[nodiscard]] game::data::RpgCatalog loadCatalog() {
    const Fixture fixture = createFixture();
    game::data::RpgCatalog catalog;
    EXPECT_TRUE(catalog.loadClasses(fixture.classes.string()));
    EXPECT_TRUE(catalog.loadActors(fixture.actors.string()));
    return catalog;
}

TEST(ActorProgressionServiceTest, ExpCurveIsMonotonicAndMapsThresholdsToLevels) {
    const game::data::RpgCatalog catalog = loadCatalog();
    const auto* actor = catalog.findActor("actor.hero");
    ASSERT_NE(actor, nullptr);

    EXPECT_EQ(ActorProgressionService::expForLevel(catalog, *actor, 1), 0);
    int previous = 0;
    for (int level = 2; level <= actor->max_level_; ++level) {
        const int current = ActorProgressionService::expForLevel(catalog, *actor, level);
        EXPECT_GT(current, previous) << level;
        EXPECT_EQ(ActorProgressionService::levelForExp(catalog, *actor, current), level);
        EXPECT_EQ(ActorProgressionService::levelForExp(catalog, *actor, current - 1), level - 1);
        previous = current;
    }
    EXPECT_EQ(ActorProgressionService::expToNextLevel(
                  catalog, *actor, ActorProgressionService::expForLevel(catalog, *actor, actor->max_level_)),
              0);
}

TEST(ActorProgressionServiceTest, GrantsExperienceLevelsUpAndAddsMaxStatDelta) {
    const game::data::RpgCatalog catalog = loadCatalog();
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.states_by_actor_id_["actor.hero"] = game::component::ActorRuntimeState{
        .current_hp = 50,
        .current_mp = 5,
        .level = 1,
        .total_exp = 0,
    };

    const auto* actor = catalog.findActor("actor.hero");
    ASSERT_NE(actor, nullptr);
    const int level_two_exp = ActorProgressionService::expForLevel(catalog, *actor, 2);

    const PartyExperienceGrantResult result = ActorProgressionService::grantExperience(
        registry,
        player,
        catalog,
        {"actor.hero", "actor.hero"},
        level_two_exp);

    ASSERT_EQ(result.actors.size(), 1U);
    EXPECT_TRUE(result.runtime_state_changed);
    EXPECT_TRUE(result.anyLevelUp());
    EXPECT_EQ(result.actors[0].old_level, 1);
    EXPECT_EQ(result.actors[0].new_level, 2);
    EXPECT_GT(result.actors[0].hp_max_delta, 0);
    EXPECT_GT(result.actors[0].mp_max_delta, 0);

    const auto& stored = registry.get<game::component::PartyRuntimeStatsComponent>(player)
                             .states_by_actor_id_.at("actor.hero");
    EXPECT_EQ(stored.level, 2);
    EXPECT_EQ(stored.total_exp, level_two_exp);
    EXPECT_EQ(stored.current_hp, 50 + result.actors[0].hp_max_delta);
    EXPECT_EQ(stored.current_mp, 5 + result.actors[0].mp_max_delta);
}

TEST(ActorProgressionServiceTest, GrantExperienceUsesCustomPlayerDisplayNameForDefaultActor) {
    const game::data::RpgCatalog catalog = loadCatalog();
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<game::component::PlayerIdentityComponent>(
        player,
        game::component::PlayerIdentityComponent{.display_name_ = "Mina"});
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 50,
        .current_mp = 5,
        .level = 1,
        .total_exp = 0,
    };

    const auto* actor = catalog.findActor("actor.player");
    ASSERT_NE(actor, nullptr);
    const int level_two_exp = ActorProgressionService::expForLevel(catalog, *actor, 2);

    const PartyExperienceGrantResult result = ActorProgressionService::grantExperience(
        registry,
        player,
        catalog,
        {"actor.player"},
        level_two_exp);

    ASSERT_EQ(result.actors.size(), 1U);
    EXPECT_EQ(result.actors[0].actor_id, "actor.player");
    EXPECT_EQ(result.actors[0].display_name, "Mina");
    EXPECT_TRUE(result.actors[0].leveledUp());
}

TEST(ActorProgressionServiceTest, MaxLevelClampsTotalExp) {
    const game::data::RpgCatalog catalog = loadCatalog();
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    auto& runtime = registry.emplace<game::component::PartyRuntimeStatsComponent>(player);
    const auto* actor = catalog.findActor("actor.hero");
    ASSERT_NE(actor, nullptr);
    const int max_exp = ActorProgressionService::expForLevel(catalog, *actor, actor->max_level_);
    runtime.states_by_actor_id_["actor.hero"] = game::component::ActorRuntimeState{
        .current_hp = 1,
        .current_mp = 0,
        .level = actor->max_level_,
        .total_exp = max_exp,
    };

    const PartyExperienceGrantResult result = ActorProgressionService::grantExperience(
        registry,
        player,
        catalog,
        {"actor.hero"},
        999999);

    ASSERT_EQ(result.actors.size(), 1U);
    EXPECT_FALSE(result.runtime_state_changed);
    EXPECT_EQ(result.actors[0].gained_exp, 0);
    const auto& stored = registry.get<game::component::PartyRuntimeStatsComponent>(player)
                             .states_by_actor_id_.at("actor.hero");
    EXPECT_EQ(stored.level, actor->max_level_);
    EXPECT_EQ(stored.total_exp, max_exp);
}

TEST(ActorProgressionServiceTest, PreviewExperienceClampsLargeRewardWithoutOverflow) {
    const game::data::RpgCatalog catalog = loadCatalog();
    const auto* actor = catalog.findActor("actor.hero");
    ASSERT_NE(actor, nullptr);

    const int max_exp = ActorProgressionService::expForLevel(catalog, *actor, actor->max_level_);
    const int almost_max_exp = max_exp - 1;
    std::unordered_map<std::string, game::component::ActorRuntimeState> runtime_states{};
    runtime_states["actor.hero"] = game::component::ActorRuntimeState{
        .current_hp = 1,
        .current_mp = 0,
        .level = ActorProgressionService::levelForExp(catalog, *actor, almost_max_exp),
        .total_exp = almost_max_exp,
    };
    const std::unordered_map<std::string, game::component::ActorEquipmentLoadout> equipment{};

    const PartyExperienceGrantResult result = ActorProgressionService::previewExperience(
        catalog,
        {"actor.hero"},
        runtime_states,
        equipment,
        std::numeric_limits<int>::max());

    ASSERT_EQ(result.actors.size(), 1U);
    EXPECT_EQ(result.actors[0].total_exp, max_exp);
    EXPECT_EQ(result.actors[0].gained_exp, 1);
}

TEST(ActorProgressionServiceTest, PreviewExperienceUsesDisplayNameOverrides) {
    const game::data::RpgCatalog catalog = loadCatalog();
    const auto* actor = catalog.findActor("actor.player");
    ASSERT_NE(actor, nullptr);

    const int level_two_exp = ActorProgressionService::expForLevel(catalog, *actor, 2);
    std::unordered_map<std::string, game::component::ActorRuntimeState> runtime_states{};
    runtime_states["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 50,
        .current_mp = 5,
        .level = 1,
        .total_exp = 0,
    };
    const std::unordered_map<std::string, game::component::ActorEquipmentLoadout> equipment{};
    const std::unordered_map<std::string, std::string> display_name_overrides{
        {"actor.player", "Mina"},
    };

    const PartyExperienceGrantResult result = ActorProgressionService::previewExperience(
        catalog,
        {"actor.player"},
        runtime_states,
        equipment,
        level_two_exp,
        display_name_overrides);

    ASSERT_EQ(result.actors.size(), 1U);
    EXPECT_EQ(result.actors[0].display_name, "Mina");
    EXPECT_TRUE(result.actors[0].leveledUp());
}

TEST(ActorProgressionServiceTest, NormalizePreservesZeroHp) {
    const game::data::RpgCatalog catalog = loadCatalog();
    const auto* actor = catalog.findActor("actor.hero");
    ASSERT_NE(actor, nullptr);

    const auto normalized = ActorProgressionService::normalizeState(
        catalog,
        *actor,
        game::component::ActorRuntimeState{
            .current_hp = 0,
            .current_mp = 0,
            .level = 1,
            .total_exp = 0,
        },
        nullptr);

    EXPECT_EQ(normalized.current_hp, 0);
    EXPECT_EQ(normalized.current_mp, 0);
    EXPECT_EQ(normalized.level, 1);
}

TEST(ActorProgressionServiceTest, NormalizeUsesTotalExpAsLevelSource) {
    const game::data::RpgCatalog catalog = loadCatalog();
    const auto* actor = catalog.findActor("actor.hero");
    ASSERT_NE(actor, nullptr);

    const auto normalized = ActorProgressionService::normalizeState(
        catalog,
        *actor,
        game::component::ActorRuntimeState{
            .current_hp = 10,
            .current_mp = 3,
            .level = 5,
            .total_exp = 0,
        },
        nullptr);

    EXPECT_EQ(normalized.level, actor->initial_level_);
    EXPECT_EQ(normalized.total_exp, ActorProgressionService::expForLevel(catalog, *actor, actor->initial_level_));
}

TEST(ActorProgressionServiceTest, MissingParamCurvesKeepBaseParamsForEveryLevel) {
    const game::data::RpgCatalog catalog = loadCatalog();
    const auto* actor = catalog.findActor("actor.flat");
    ASSERT_NE(actor, nullptr);

    const auto level_one = game::battle::resolveActorStats(catalog, *actor, 1, nullptr);
    const auto level_ten = game::battle::resolveActorStats(catalog, *actor, 10, nullptr);

    EXPECT_EQ(level_one.params, level_ten.params);
}

} // namespace
} // namespace game::domain
// NOLINTEND
