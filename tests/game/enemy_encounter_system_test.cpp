#include <gtest/gtest.h>

#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/spatial/spatial_index_manager.h"
#include "game/component/enemy_encounter_component.h"
#include "game/component/map_component.h"
#include "game/component/tags.h"
#include "game/defs/commands.h"
#include "game/system/enemy_encounter_system.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <string>
#include <vector>

namespace game::system {
namespace {

using namespace entt::literals;

constexpr entt::id_type TEST_MAP_ID = "test_map"_hs;

struct EnterBattleCommandCollector {
    std::vector<game::defs::EnterBattleCommand> commands{};

    void onCommand(const game::defs::EnterBattleCommand& command) {
        commands.push_back(command);
    }
};

void initSpatialIndex(entt::registry& registry, engine::spatial::SpatialIndexManager& spatial_index) {
    spatial_index.initialize(
        registry,
        glm::ivec2{16, 16},
        glm::ivec2{16, 16},
        glm::vec2{0.0F, 0.0F},
        glm::vec2{256.0F, 256.0F});
}

entt::entity createPlayer(entt::registry& registry, glm::vec2 position, entt::id_type map_id = TEST_MAP_ID) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<game::component::MapId>(player, map_id);
    registry.emplace<engine::component::TransformComponent>(player, position);
    registry.emplace<engine::component::CircleCollider>(player, 8.0F, glm::vec2{0.0F, 0.0F});
    registry.emplace<engine::component::SpatialIndexTag>(player);
    return player;
}

entt::entity createEnemy(entt::registry& registry,
                         glm::vec2 position,
                         int encounter_id,
                         entt::id_type map_id = TEST_MAP_ID) {
    const std::string troop_id = "troop.slime";
    const entt::entity enemy = registry.create();
    registry.emplace<game::component::MapId>(enemy, map_id);
    registry.emplace<engine::component::TransformComponent>(enemy, position);
    registry.emplace<engine::component::CircleCollider>(enemy, 8.0F, glm::vec2{0.0F, 0.0F});
    registry.emplace<engine::component::SpatialIndexTag>(enemy);
    registry.emplace<game::component::EnemyEncounterComponent>(
        enemy,
        game::component::EnemyEncounterComponent{
            .troop_id_ = troop_id,
            .troop_id_hash_ = entt::hashed_string{troop_id.c_str()}.value(),
            .battle_background_id_ = "Grassland",
            .encounter_id_ = encounter_id,
            .respawn_on_map_reload_ = false,
            .home_position_ = position});
    return enemy;
}

TEST(EnemyEncounterSystemTest, TouchingEnemyTriggersEnterBattleCommand) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::world::WorldState world_state;
    world_state.setCurrentMap(TEST_MAP_ID);
    engine::spatial::SpatialIndexManager spatial_index;
    initSpatialIndex(registry, spatial_index);

    const entt::entity player = createPlayer(registry, glm::vec2{16.0F, 16.0F});
    const entt::entity enemy = createEnemy(registry, glm::vec2{35.0F, 16.0F}, 1001);
    spatial_index.addColliderEntity(player);
    spatial_index.addColliderEntity(enemy);

    EnterBattleCommandCollector collector;
    dispatcher.sink<game::defs::EnterBattleCommand>().connect<&EnterBattleCommandCollector::onCommand>(&collector);

    EnemyEncounterSystem system(registry, dispatcher, spatial_index, world_state, nullptr);
    system.update(1.0F / 60.0F);

    ASSERT_EQ(collector.commands.size(), 1U);
    EXPECT_EQ(collector.commands[0].troop_id, "troop.slime");
    EXPECT_EQ(collector.commands[0].battle_background_id, "Grassland");
    ASSERT_TRUE(collector.commands[0].encounter_context.has_value());
    EXPECT_EQ(collector.commands[0].encounter_context->source_entity, enemy);
    EXPECT_EQ(collector.commands[0].encounter_context->encounter_id, 1001);
    EXPECT_FALSE(collector.commands[0].encounter_context->respawn_on_map_reload);
    EXPECT_TRUE(registry.get<game::component::EnemyEncounterComponent>(enemy).engaged_);
}

TEST(EnemyEncounterSystemTest, EngagedEnemyDoesNotRetriggerOnLaterUpdates) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::world::WorldState world_state;
    world_state.setCurrentMap(TEST_MAP_ID);
    engine::spatial::SpatialIndexManager spatial_index;
    initSpatialIndex(registry, spatial_index);

    const entt::entity player = createPlayer(registry, glm::vec2{16.0F, 16.0F});
    const entt::entity enemy = createEnemy(registry, glm::vec2{35.0F, 16.0F}, 1001);
    spatial_index.addColliderEntity(player);
    spatial_index.addColliderEntity(enemy);

    EnterBattleCommandCollector collector;
    dispatcher.sink<game::defs::EnterBattleCommand>().connect<&EnterBattleCommandCollector::onCommand>(&collector);

    EnemyEncounterSystem system(registry, dispatcher, spatial_index, world_state, nullptr);
    system.update(1.0F / 60.0F);
    system.update(1.0F / 60.0F);

    EXPECT_EQ(collector.commands.size(), 1U);
}

TEST(EnemyEncounterSystemTest, CooldownBlocksTriggerUntilElapsed) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::world::WorldState world_state;
    world_state.setCurrentMap(TEST_MAP_ID);
    engine::spatial::SpatialIndexManager spatial_index;
    initSpatialIndex(registry, spatial_index);

    const entt::entity player = createPlayer(registry, glm::vec2{16.0F, 16.0F});
    const entt::entity enemy = createEnemy(registry, glm::vec2{35.0F, 16.0F}, 1001);
    registry.get<game::component::EnemyEncounterComponent>(enemy).cooldown_timer_ = 0.25F;
    spatial_index.addColliderEntity(player);
    spatial_index.addColliderEntity(enemy);

    EnterBattleCommandCollector collector;
    dispatcher.sink<game::defs::EnterBattleCommand>().connect<&EnterBattleCommandCollector::onCommand>(&collector);

    EnemyEncounterSystem system(registry, dispatcher, spatial_index, world_state, nullptr);
    system.update(0.1F);
    EXPECT_TRUE(collector.commands.empty());

    system.update(0.2F);
    EXPECT_EQ(collector.commands.size(), 1U);
}

TEST(EnemyEncounterSystemTest, IgnoresEnemiesOnDifferentMap) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::world::WorldState world_state;
    world_state.setCurrentMap(TEST_MAP_ID);
    engine::spatial::SpatialIndexManager spatial_index;
    initSpatialIndex(registry, spatial_index);

    const entt::entity player = createPlayer(registry, glm::vec2{16.0F, 16.0F});
    const entt::entity enemy = createEnemy(registry, glm::vec2{35.0F, 16.0F}, 1001, "other_map"_hs);
    spatial_index.addColliderEntity(player);
    spatial_index.addColliderEntity(enemy);

    EnterBattleCommandCollector collector;
    dispatcher.sink<game::defs::EnterBattleCommand>().connect<&EnterBattleCommandCollector::onCommand>(&collector);

    EnemyEncounterSystem system(registry, dispatcher, spatial_index, world_state, nullptr);
    system.update(1.0F / 60.0F);

    EXPECT_TRUE(collector.commands.empty());
}

} // namespace
} // namespace game::system
