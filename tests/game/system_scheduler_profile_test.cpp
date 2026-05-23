// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/runtime/game_mode.h"
#include "game/runtime/system_scheduler.h"
#include "game/runtime/system_bundle.h"
#include "game/system/day_night_system.h"
#include "game/data/game_time.h"
#include "engine/render/lighting_state.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace game::runtime {
namespace {

[[nodiscard]] size_t indexOf(const std::vector<SchedulerStage>& stages, const SchedulerStage target) {
    const auto it = std::find(stages.begin(), stages.end(), target);
    EXPECT_NE(it, stages.end()) << "missing stage: " << toString(target);
    if (it == stages.end()) {
        return stages.size();
    }
    return static_cast<size_t>(std::distance(stages.begin(), it));
}

[[nodiscard]] std::vector<SchedulerStage> traceStages(const SystemScheduler::TickResult& result) {
    std::vector<SchedulerStage> stages;
    stages.reserve(result.trace.stages.size());
    for (const auto& sample : result.trace.stages) {
        stages.push_back(sample.stage);
    }
    return stages;
}

TEST(SystemSchedulerProfileTest, ExplorationKeepsMovementBeforeSpatialIndex) {
    const auto& stages = SystemScheduler::profileStages(GameMode::Exploration);
    ASSERT_FALSE(stages.empty());

    const size_t movement = indexOf(stages, SchedulerStage::Movement);
    const size_t spatial_index = indexOf(stages, SchedulerStage::SpatialIndex);
    EXPECT_LT(movement, spatial_index);
}

TEST(SystemSchedulerProfileTest, ExplorationKeepsTimeBeforeDayNight) {
    const auto& stages = SystemScheduler::profileStages(GameMode::Exploration);
    ASSERT_FALSE(stages.empty());

    const size_t time = indexOf(stages, SchedulerStage::Time);
    const size_t day_night = indexOf(stages, SchedulerStage::DayNight);
    EXPECT_LT(time, day_night);
}

TEST(SystemSchedulerProfileTest, ExplorationKeepsQuestInteractionBetweenDialogueAndAutoTile) {
    const auto& stages = SystemScheduler::profileStages(GameMode::Exploration);
    ASSERT_FALSE(stages.empty());

    const size_t dialogue = indexOf(stages, SchedulerStage::Dialogue);
    const size_t quest_interaction = indexOf(stages, SchedulerStage::QuestInteraction);
    const size_t auto_tile = indexOf(stages, SchedulerStage::AutoTile);

    EXPECT_LT(dialogue, quest_interaction);
    EXPECT_LT(quest_interaction, auto_tile);
}

TEST(SystemSchedulerProfileTest, ExplorationDrainsScriptCommandsAfterStateBeforeMovement) {
    const auto& stages = SystemScheduler::profileStages(GameMode::Exploration);
    ASSERT_FALSE(stages.empty());

    const size_t state = indexOf(stages, SchedulerStage::State);
    const size_t script_commands = indexOf(stages, SchedulerStage::ScriptCommands);
    const size_t movement = indexOf(stages, SchedulerStage::Movement);

    EXPECT_LT(state, script_commands);
    EXPECT_LT(script_commands, movement);
}

TEST(SystemSchedulerProfileTest, BattleAndPauseProfilesOnlyRunCleanupStage) {
    const auto& battle = SystemScheduler::profileStages(GameMode::Battle);
    const auto& pause = SystemScheduler::profileStages(GameMode::PauseOverlay);

    ASSERT_EQ(battle.size(), 1U);
    ASSERT_EQ(pause.size(), 1U);
    EXPECT_EQ(battle.front(), SchedulerStage::RemoveEntity);
    EXPECT_EQ(pause.front(), SchedulerStage::RemoveEntity);
}

TEST(SystemSchedulerProfileTest, ExplorationRunsEnemyEncounterAfterSpatialBeforeInteraction) {
    const auto& stages = SystemScheduler::profileStages(GameMode::Exploration);
    ASSERT_FALSE(stages.empty());

    const size_t spatial_index = indexOf(stages, SchedulerStage::SpatialIndex);
    const size_t enemy_encounter = indexOf(stages, SchedulerStage::EnemyEncounter);
    const size_t interaction = indexOf(stages, SchedulerStage::Interaction);

    EXPECT_LT(spatial_index, enemy_encounter);
    EXPECT_LT(enemy_encounter, interaction);
}

TEST(SystemSchedulerProfileTest, PostGateDotDumpComesFromDeclaredParallelStages) {
    const std::string dot = dumpPostGateParallelIslandDot();

    EXPECT_NE(dot.find("SpatialIndex"), std::string::npos);
    EXPECT_NE(dot.find("CameraFollow"), std::string::npos);
    EXPECT_NE(dot.find("Animation"), std::string::npos);
}

TEST(SystemSchedulerProfileTest, ExplorationTickMatchesProfileOrderWhenNoTransition) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    GameSystemBundle systems;
    SystemScheduler scheduler;

    const auto result = scheduler.tick({
        .mode = GameMode::Exploration,
        .systems = systems,
        .registry = registry,
        .dispatcher = &dispatcher,
        .delta_time = 0.016f,
        .is_transition_active = []() { return false; }
    });

    const auto executed = traceStages(result);

    const auto& profile = SystemScheduler::profileStages(GameMode::Exploration);
    std::vector<SchedulerStage> expected;
    expected.reserve(profile.size());
    for (const auto stage : profile) {
        if (stage == SchedulerStage::TransitionUpdatePre || stage == SchedulerStage::LightTogglePre) {
            continue;
        }
        expected.push_back(stage);
    }

    EXPECT_FALSE(result.gate1_triggered);
    EXPECT_FALSE(result.gate2_triggered);
    EXPECT_EQ(executed, expected);
}

TEST(SystemSchedulerProfileTest, ExplorationParallelIslandRemainsStableAcrossManyTicks) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    GameSystemBundle systems;
    SystemScheduler scheduler;

    constexpr int kIterations = 2000;
    for (int i = 0; i < kIterations; ++i) {
        const auto result = scheduler.tick({
            .mode = GameMode::Exploration,
            .systems = systems,
            .registry = registry,
            .dispatcher = &dispatcher,
            .delta_time = 0.016f,
            .is_transition_active = []() { return false; }
        });

        EXPECT_FALSE(result.gate1_triggered);
        EXPECT_FALSE(result.gate2_triggered);
        ASSERT_FALSE(result.trace.stages.empty());
    }
}

TEST(SystemSchedulerProfileTest, ExplorationParallelDayNightUpdatesGlobalLightingState) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    GameSystemBundle systems;
    SystemScheduler scheduler;

    registry.ctx().emplace<game::data::GameTime>();
    systems.day_night_system = std::make_unique<game::system::DayNightSystem>(registry);

    const auto result = scheduler.tick({
        .mode = GameMode::Exploration,
        .systems = systems,
        .registry = registry,
        .dispatcher = &dispatcher,
        .delta_time = 0.016f,
        .is_transition_active = []() { return false; }
    });

    const auto executed = traceStages(result);
    EXPECT_NE(std::find(executed.begin(), executed.end(), SchedulerStage::DayNight), executed.end());

    const auto* lighting = registry.ctx().find<engine::render::GlobalLightingState>();
    ASSERT_NE(lighting, nullptr);
    EXPECT_FALSE(lighting->directional_lights.empty());
}

} // namespace
} // namespace game::runtime
// NOLINTEND
