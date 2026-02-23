// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/system/deferred_commands.h"
#include "game/component/actor_component.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/data/game_time.h"
#include "game/system/animal_behavior_system.h"
#include "game/system/npc_wander_system.h"

#include <entt/entity/registry.hpp>
#include <glm/vec2.hpp>

namespace game::system {
namespace {

TEST(NpcAnimalParallelDomainTest, NpcWanderSystemSkipsAnimalTaggedEntities) {
    entt::registry registry;
    NPCWanderSystem npc_wander_system(registry);

    const auto animal = registry.create();
    registry.emplace<game::component::NPCTag>(animal);
    registry.emplace<game::component::AnimalTag>(animal);
    auto& animal_wander = registry.emplace<game::component::WanderComponent>(animal);
    animal_wander.phase_ = game::component::WanderPhase::Moving;
    animal_wander.target_ = glm::vec2(100.0f, 0.0f);
    registry.emplace<engine::component::TransformComponent>(animal, glm::vec2(0.0f, 0.0f));
    auto& animal_velocity = registry.emplace<engine::component::VelocityComponent>(animal);
    animal_velocity.velocity_ = glm::vec2(3.0f, 4.0f);
    registry.emplace<game::component::ActorComponent>(animal, game::component::ActorComponent{4.0f});
    registry.emplace<game::component::StateComponent>(animal, game::component::StateComponent{});
    registry.emplace<game::component::SleepRoutine>(animal, game::component::SleepRoutine{true, true});

    const auto npc = registry.create();
    registry.emplace<game::component::NPCTag>(npc);
    auto& npc_wander = registry.emplace<game::component::WanderComponent>(npc);
    npc_wander.phase_ = game::component::WanderPhase::Waiting;
    npc_wander.wait_timer_ = 1.0f;
    registry.emplace<engine::component::TransformComponent>(npc, glm::vec2(0.0f, 0.0f));
    registry.emplace<engine::component::VelocityComponent>(npc, engine::component::VelocityComponent{});
    registry.emplace<game::component::ActorComponent>(npc, game::component::ActorComponent{2.0f});
    registry.emplace<game::component::StateComponent>(npc, game::component::StateComponent{});

    engine::system::DeferredCommands deferred;
    npc_wander_system.update(0.016f, deferred);
    deferred.drain(registry);

    EXPECT_FLOAT_EQ(animal_velocity.velocity_.x, 3.0f);
    EXPECT_FLOAT_EQ(animal_velocity.velocity_.y, 4.0f);
    EXPECT_EQ(animal_wander.phase_, game::component::WanderPhase::Moving);
}

TEST(NpcAnimalParallelDomainTest, AnimalBehaviorSystemKeepsWanderMovementWhenNotEatingOrSleeping) {
    entt::registry registry;
    auto& game_time = registry.ctx().emplace<game::data::GameTime>();
    game_time.time_of_day_ = game::data::TimeOfDay::Day;

    AnimalBehaviorSystem animal_behavior_system(registry);

    const auto animal = registry.create();
    registry.emplace<game::component::AnimalTag>(animal);
    registry.emplace<game::component::SleepRoutine>(animal, game::component::SleepRoutine{true, false});
    auto& state = registry.emplace<game::component::StateComponent>(animal);
    auto& behavior = registry.emplace<game::component::AnimalBehaviorState>(animal);
    behavior.phase_ = game::component::AnimalBehaviorPhase::Wander;
    behavior.eat_cooldown_timer_ = 10.0f;
    auto& wander = registry.emplace<game::component::WanderComponent>(animal);
    wander.phase_ = game::component::WanderPhase::Moving;
    wander.target_ = glm::vec2(10.0f, 0.0f);
    wander.last_distance_sq_ = 100.0f;
    wander.radius_ = 10.0f;
    registry.emplace<game::component::ActorComponent>(animal, game::component::ActorComponent{3.0f});
    registry.emplace<engine::component::TransformComponent>(animal, glm::vec2(0.0f, 0.0f));
    auto& velocity = registry.emplace<engine::component::VelocityComponent>(animal);
    velocity.velocity_ = glm::vec2(0.0f, 0.0f);

    engine::system::DeferredCommands deferred;
    animal_behavior_system.update(0.016f, deferred);
    deferred.drain(registry);

    EXPECT_EQ(behavior.phase_, game::component::AnimalBehaviorPhase::Wander);
    EXPECT_GT(velocity.velocity_.x, 0.0f);
    EXPECT_EQ(state.action_, game::component::Action::Walk);
}

TEST(NpcAnimalParallelDomainTest, AnimalBehaviorSystemCanEnterEatingPhaseAndStopWander) {
    entt::registry registry;
    auto& game_time = registry.ctx().emplace<game::data::GameTime>();
    game_time.time_of_day_ = game::data::TimeOfDay::Day;

    AnimalBehaviorSystem animal_behavior_system(registry);

    const auto animal = registry.create();
    registry.emplace<game::component::AnimalTag>(animal);
    registry.emplace<game::component::SleepRoutine>(animal, game::component::SleepRoutine{true, false});
    auto& state = registry.emplace<game::component::StateComponent>(animal);
    auto& behavior = registry.emplace<game::component::AnimalBehaviorState>(animal);
    behavior.phase_ = game::component::AnimalBehaviorPhase::Wander;
    behavior.eat_cooldown_timer_ = 0.0f;
    behavior.eat_duration_ = 1.5f;
    auto& wander = registry.emplace<game::component::WanderComponent>(animal);
    wander.phase_ = game::component::WanderPhase::Moving;
    wander.radius_ = 10.0f;
    registry.emplace<game::component::ActorComponent>(animal, game::component::ActorComponent{3.0f});
    registry.emplace<engine::component::TransformComponent>(animal, glm::vec2(0.0f, 0.0f));
    auto& velocity = registry.emplace<engine::component::VelocityComponent>(animal);
    velocity.velocity_ = glm::vec2(1.0f, 0.0f);

    engine::system::DeferredCommands deferred;
    animal_behavior_system.update(0.016f, deferred);
    deferred.drain(registry);

    EXPECT_EQ(behavior.phase_, game::component::AnimalBehaviorPhase::Eating);
    EXPECT_EQ(wander.phase_, game::component::WanderPhase::Waiting);
    EXPECT_FLOAT_EQ(velocity.velocity_.x, 0.0f);
    EXPECT_FLOAT_EQ(velocity.velocity_.y, 0.0f);
    EXPECT_EQ(state.action_, game::component::Action::Eat);
}

} // namespace
} // namespace game::system
// NOLINTEND
