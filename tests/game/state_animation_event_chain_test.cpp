#include <gtest/gtest.h>

#include "engine/component/animation_component.h"
#include "engine/component/sprite_component.h"
#include "engine/system/animation_system.h"
#include "engine/system/deferred_commands.h"
#include "engine/system/task_event_buffer.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/system/state_system.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <unordered_map>
#include <vector>

using namespace entt::literals;

namespace {

TEST(StateAnimationEventChainTest, DeferredAndTaskEventBufferPreserveStateAnimationCascade) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    game::system::StateSystem state_system(registry, dispatcher);
    engine::system::AnimationSystem animation_system(registry, dispatcher);

    const entt::entity entity = registry.create();

    auto& state = registry.emplace<game::component::StateComponent>(entity);
    state.action_ = game::component::Action::Hoe;
    state.direction_ = game::component::Direction::Down;
    registry.emplace<game::component::StateDirtyTag>(entity);
    registry.emplace<game::component::ActionLockedTag>(entity);

    const entt::id_type hoe_down_id = "hoe_down"_hs;
    const entt::id_type idle_down_id = "idle_down"_hs;
    const entt::id_type texture_id = "test_texture"_hs;

    engine::component::Animation hoe_anim{};
    hoe_anim.texture_id_ = texture_id;
    hoe_anim.dst_size_ = glm::vec2{16.0f, 16.0f};
    hoe_anim.frames_.push_back(engine::component::AnimationFrame{
        engine::utils::Rect{0.0f, 0.0f, 16.0f, 16.0f},
        1.0f
    });

    engine::component::Animation idle_anim = hoe_anim;
    idle_anim.loop_ = true;

    std::unordered_map<entt::id_type, engine::component::Animation> animations;
    animations.emplace(hoe_down_id, hoe_anim);
    animations.emplace(idle_down_id, idle_anim);

    registry.emplace<engine::component::AnimationComponent>(entity, std::move(animations), idle_down_id);
    registry.emplace<engine::component::SpriteComponent>(
        entity,
        engine::component::Sprite{texture_id, engine::utils::Rect{0.0f, 0.0f, 16.0f, 16.0f}});

    engine::system::DeferredCommands deferred;
    engine::system::TaskEventBuffer state_events;
    state_system.update(deferred, state_events);

    EXPECT_TRUE(registry.any_of<game::component::StateDirtyTag>(entity));
    deferred.drain(registry);
    EXPECT_FALSE(registry.any_of<game::component::StateDirtyTag>(entity));

    state_events.flushTo(dispatcher);
    dispatcher.update();

    const auto& after_play = registry.get<engine::component::AnimationComponent>(entity);
    EXPECT_EQ(after_play.current_animation_id_, hoe_down_id);
    EXPECT_EQ(after_play.current_frame_index_, 0U);

    engine::system::TaskEventBuffer animation_events;
    animation_system.update(0.01f, animation_events);
    animation_events.flushTo(dispatcher);
    dispatcher.update();

    EXPECT_FALSE(registry.any_of<game::component::ActionLockedTag>(entity));
    const auto& after_finished = registry.get<game::component::StateComponent>(entity);
    EXPECT_EQ(after_finished.action_, game::component::Action::Idle);
    EXPECT_TRUE(registry.any_of<game::component::StateDirtyTag>(entity));
}

} // namespace
