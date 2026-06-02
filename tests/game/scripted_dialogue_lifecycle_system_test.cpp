#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/system/deferred_commands.h"
#include "game/component/actor_component.h"
#include "game/component/npc_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/defs/events_dialogue.h"
#include "game/system/npc_wander_system.h"
#include "game/system/scripted_dialogue_lifecycle_system.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <vector>

namespace {

struct DialogueHideCapture {
    std::vector<game::defs::DialogueHideEvent> hides{};

    void onHide(const game::defs::DialogueHideEvent& event) { hides.push_back(event); }
};

struct ScriptedDialogueLifecycleEnv {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::system::ScriptedDialogueLifecycleSystem system;
    DialogueHideCapture capture{};
    entt::entity player{entt::null};
    entt::entity target{entt::null};

    ScriptedDialogueLifecycleEnv() : system(registry, dispatcher) {
        player = registry.create();
        registry.emplace<game::component::PlayerTag>(player);
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});

        target = registry.create();
        registry.emplace<engine::component::TransformComponent>(target, glm::vec2{32.0F, 0.0F});
        registry.emplace<game::component::ScriptedInteractionComponent>(target);
        auto& dialogue = registry.emplace<game::component::DialogueComponent>(target);
        dialogue.interact_distance_ = 80.0F;

        dispatcher.sink<game::defs::DialogueHideEvent>().connect<&DialogueHideCapture::onHide>(&capture);
    }

    ~ScriptedDialogueLifecycleEnv() { dispatcher.disconnect(&capture); }

    void showConversation(entt::entity entity) {
        game::defs::DialogueShowEvent event{};
        event.target = entity;
        event.text = "Hello";
        event.world_position = glm::vec2{32.0F, 0.0F};
        event.channel = game::defs::DialogueChannel::Conversation;
        dispatcher.trigger(event);
    }
};

} // namespace

namespace game::system {

TEST(ScriptedDialogueLifecycleSystemTest, ClosesScriptedConversationWhenPlayerWalksAway) {
    ScriptedDialogueLifecycleEnv env{};

    env.showConversation(env.target);
    EXPECT_TRUE(env.registry.get<game::component::DialogueComponent>(env.target).active_);

    env.system.update(0.016F);
    env.dispatcher.update();
    EXPECT_TRUE(env.capture.hides.empty());

    env.registry.get<engine::component::TransformComponent>(env.player).position_ = glm::vec2{-16.0F, 0.0F};
    env.system.update(0.016F);
    env.dispatcher.update();
    EXPECT_TRUE(env.capture.hides.empty());

    env.registry.get<engine::component::TransformComponent>(env.player).position_ = glm::vec2{-17.0F, 0.0F};
    env.system.update(0.016F);
    EXPECT_FALSE(env.registry.get<game::component::DialogueComponent>(env.target).active_);

    env.dispatcher.update();
    ASSERT_EQ(env.capture.hides.size(), 1U);
    EXPECT_EQ(env.capture.hides.front().target, env.target);
    EXPECT_EQ(env.capture.hides.front().channel, game::defs::DialogueChannel::Conversation);
}

TEST(ScriptedDialogueLifecycleSystemTest, MarksScriptedNpcConversationActiveWhenDialogueComponentIsAbsent) {
    ScriptedDialogueLifecycleEnv env{};

    const entt::entity npc = env.registry.create();
    env.registry.emplace<engine::component::TransformComponent>(npc, glm::vec2{32.0F, 0.0F});
    env.registry.emplace<game::component::ScriptedInteractionComponent>(npc);
    env.registry.emplace<game::component::NPCTag>(npc);
    auto& wander = env.registry.emplace<game::component::WanderComponent>(npc);
    wander.phase_ = game::component::WanderPhase::Moving;
    wander.target_ = glm::vec2{100.0F, 0.0F};
    wander.radius_ = 16.0F;
    auto& velocity = env.registry.emplace<engine::component::VelocityComponent>(npc);
    velocity.velocity_ = glm::vec2{4.0F, 0.0F};
    env.registry.emplace<game::component::ActorComponent>(npc, game::component::ActorComponent{2.0F});
    auto& state = env.registry.emplace<game::component::StateComponent>(npc);
    state.action_ = game::component::Action::Walk;

    env.showConversation(npc);
    ASSERT_TRUE(env.registry.all_of<game::component::DialogueComponent>(npc));
    EXPECT_TRUE(env.registry.get<game::component::DialogueComponent>(npc).active_);

    game::system::NPCWanderSystem wander_system(env.registry);
    engine::system::DeferredCommands deferred;
    wander_system.update(0.016F, deferred);
    deferred.drain(env.registry);

    EXPECT_EQ(wander.phase_, game::component::WanderPhase::Waiting);
    EXPECT_FLOAT_EQ(velocity.velocity_.x, 0.0F);
    EXPECT_FLOAT_EQ(velocity.velocity_.y, 0.0F);
    EXPECT_EQ(state.action_, game::component::Action::Idle);
}

TEST(ScriptedDialogueLifecycleSystemTest, IgnoresNonScriptedTargetsAndNoticeChannels) {
    ScriptedDialogueLifecycleEnv env{};

    const entt::entity plain_target = env.registry.create();
    env.registry.emplace<engine::component::TransformComponent>(plain_target, glm::vec2{32.0F, 0.0F});
    env.registry.emplace<game::component::DialogueComponent>(plain_target);

    env.showConversation(plain_target);
    env.registry.get<engine::component::TransformComponent>(env.player).position_ = glm::vec2{1000.0F, 0.0F};
    env.system.update(0.016F);
    env.dispatcher.update();
    EXPECT_TRUE(env.capture.hides.empty());

    game::defs::DialogueShowEvent notice{};
    notice.target = env.target;
    notice.text = "Notice";
    notice.channel = game::defs::DialogueChannel::Notice;
    env.dispatcher.trigger(notice);
    env.system.update(0.016F);
    env.dispatcher.update();
    EXPECT_TRUE(env.capture.hides.empty());
}

} // namespace game::system
