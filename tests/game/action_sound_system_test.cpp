#include <gtest/gtest.h>

#include "engine/component/audio_component.h"
#include "engine/component/transform_component.h"
#include "engine/utils/events.h"
#include "game/component/action_sound_component.h"
#include "game/component/state_component.h"
#include "game/system/action_sound_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

using namespace entt::literals;

namespace {

struct CapturedSounds {
    std::vector<entt::id_type> triggers{};

    void onSound(const engine::utils::PlaySoundEvent& evt) { triggers.push_back(evt.sound_id_); }
};

} // namespace

namespace game::system {

TEST(ActionSoundSystemTest, EnterIdle_RespectsCooldown) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    ActionSoundSystem system(registry, dispatcher);

    CapturedSounds captured{};
    dispatcher.sink<engine::utils::PlaySoundEvent>().connect<&CapturedSounds::onSound>(&captured);

    const entt::entity entity = registry.create();
    registry.emplace<engine::component::TransformComponent>(entity, glm::vec2{0.0f, 0.0f});

    std::unordered_map<entt::id_type, entt::id_type> sound_map{{"idle"_hs, "cow_moo"_hs}};
    registry.emplace<engine::component::AudioComponent>(entity, std::move(sound_map));

    auto& state = registry.emplace<game::component::StateComponent>(entity);
    state.action_ = game::component::Action::Idle;

    game::component::ActionSoundComponent action_sounds{};
    action_sounds.last_action_ = game::component::Action::Walk;
    action_sounds.triggers_.emplace("idle"_hs, game::component::ActionSoundTriggerConfig{1.0f, 1.0f});
    registry.emplace<game::component::ActionSoundComponent>(entity, std::move(action_sounds));

    system.update(0.0f);
    dispatcher.update();
    ASSERT_EQ(captured.triggers.size(), 1u);
    EXPECT_EQ(captured.triggers[0], "idle"_hs);

    state.action_ = game::component::Action::Walk;
    system.update(0.2f);
    dispatcher.update();

    state.action_ = game::component::Action::Idle;
    system.update(0.2f);
    dispatcher.update();
    EXPECT_EQ(captured.triggers.size(), 1u);

    system.update(1.0f);
    dispatcher.update();

    state.action_ = game::component::Action::Walk;
    system.update(0.0f);
    dispatcher.update();

    state.action_ = game::component::Action::Idle;
    system.update(0.0f);
    dispatcher.update();

    ASSERT_EQ(captured.triggers.size(), 2u);
    EXPECT_EQ(captured.triggers[1], "idle"_hs);
}

} // namespace game::system

