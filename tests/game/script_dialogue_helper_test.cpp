#include <gtest/gtest.h>

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "engine/script/script_host.h"
#include "game/component/actor_identity_component.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/events_dialogue.h"
#include "game/script/script_event_bridge.h"
#include "script_test_utils.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string projectScriptRoot() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "scripts").string();
}

struct DialogueCapture {
    std::vector<std::string> shown_texts{};
    std::vector<std::string> speakers{};
    std::vector<std::string> speaker_actor_ids{};
    int hide_count{0};

    void onShow(const game::defs::DialogueShowEvent& event) {
        shown_texts.push_back(event.text);
        speakers.push_back(event.speaker);
        speaker_actor_ids.push_back(event.speaker_actor_id);
    }

    void onHide(const game::defs::DialogueHideEvent&) {
        ++hide_count;
    }
};

struct ScriptDialogueHelperEnv {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    engine::script::ScriptHost host;
    game::script::ScriptEventBridge bridge;
    entt::entity player{entt::null};
    entt::entity target{entt::null};
    entt::entity other_target{entt::null};
    DialogueCapture capture{};

    ScriptDialogueHelperEnv()
        : host(registry),
          bridge(host, registry, dispatcher) {
        player = registry.create();
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});

        target = registry.create();
        registry.emplace<engine::component::TransformComponent>(target, glm::vec2{16.0F, 0.0F});
        registry.emplace<engine::component::NameComponent>(
            target,
            engine::component::NameComponent{
                .name_id_ = entt::hashed_string{"Lyria"}.value(),
                .name_ = "Lyria",
            });
        registry.emplace<game::component::ActorIdentityComponent>(
            target,
            game::component::ActorIdentityComponent{
                .actor_id_ = "actor.lyria",
                .actor_id_hash_ = entt::hashed_string{"actor.lyria"}.value(),
                .blueprint_id_ = "lyria",
            });

        other_target = registry.create();
        registry.emplace<engine::component::TransformComponent>(other_target, glm::vec2{32.0F, 0.0F});

        dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);
        dispatcher.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&capture);

        host.setScriptRoot(projectScriptRoot());
        EXPECT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));
        host.luaState()["test_target"] = host.makeHandle(target);
        host.luaState()["test_other_target"] = host.makeHandle(other_target);
    }

    ~ScriptDialogueHelperEnv() {
        dispatcher.disconnect(&capture);
    }

    void interact() {
        interactWith(target);
    }

    void interactWith(entt::entity entity) {
        dispatcher.trigger(game::defs::InteractCommand{player, entity});
        bridge.drainDeferredCommands();
        dispatcher.update();
    }
};

} // namespace

namespace game::script {

TEST(ScriptDialogueHelperTest, InteractAdvancesSequenceAndClosesAtEnd) {
    ScriptDialogueHelperEnv env{};

    ASSERT_TRUE(env.host.exec(R"(
        local dialogue = tf.script.require("lib.dialogue")
        helper_done = nil
        helper_started = false

        assert(tf.event.on("interact", function(evt)
            if evt.target == nil or evt.target.entity_id ~= test_target.entity_id or helper_started then
                return
            end
            helper_started = true
            assert(dialogue.start(evt.target, {"Hi", "Bye"}, function(interrupted)
                helper_done = interrupted
            end) == true)
        end) == true)
    )"));

    env.interact();
    ASSERT_EQ(env.capture.shown_texts.size(), 1U);
    EXPECT_EQ(env.capture.shown_texts[0], "Hi");
    EXPECT_EQ(env.capture.speakers[0], "Lyria");
    EXPECT_EQ(env.capture.speaker_actor_ids[0], "actor.lyria");
    EXPECT_EQ(env.capture.hide_count, 0);

    env.interact();
    ASSERT_EQ(env.capture.shown_texts.size(), 2U);
    EXPECT_EQ(env.capture.shown_texts[1], "Bye");
    EXPECT_EQ(env.capture.speakers[1], "Lyria");
    EXPECT_EQ(env.capture.speaker_actor_ids[1], "actor.lyria");
    EXPECT_EQ(env.capture.hide_count, 0);

    env.interact();
    EXPECT_EQ(env.capture.shown_texts.size(), 2U);
    EXPECT_EQ(env.capture.hide_count, 1);
    EXPECT_TRUE(env.host.exec("assert(helper_done == false)"));
}

TEST(ScriptDialogueHelperTest, CancelClearsSequenceAndReportsInterrupted) {
    ScriptDialogueHelperEnv env{};

    ASSERT_TRUE(env.host.exec(R"(
        local dialogue = tf.script.require("lib.dialogue")
        cancel_done = nil
        assert(dialogue.start(test_target, {"Hi", "Bye"}, function(interrupted)
            cancel_done = interrupted
        end) == true)
    )"));

    ASSERT_EQ(env.capture.shown_texts.size(), 1U);
    EXPECT_EQ(env.capture.shown_texts[0], "Hi");

    ASSERT_TRUE(env.host.exec(R"(
        local dialogue = tf.script.require("lib.dialogue")
        assert(dialogue.cancel(test_target) == true)
    )"));
    env.bridge.drainDeferredCommands();
    env.dispatcher.update();

    EXPECT_EQ(env.capture.hide_count, 1);
    EXPECT_TRUE(env.host.exec("assert(cancel_done == true)"));

    env.interact();
    EXPECT_EQ(env.capture.shown_texts.size(), 1U);
    EXPECT_EQ(env.capture.hide_count, 1);
}

TEST(ScriptDialogueHelperTest, SwitchingTargetsCancelsActiveSequence) {
    ScriptDialogueHelperEnv env{};

    ASSERT_TRUE(env.host.exec(R"(
        local dialogue = tf.script.require("lib.dialogue")
        switched_done = nil
        assert(dialogue.start(test_target, {"Hi", "Bye"}, function(interrupted)
            switched_done = interrupted
        end) == true)
    )"));

    ASSERT_EQ(env.capture.shown_texts.size(), 1U);
    EXPECT_EQ(env.capture.shown_texts[0], "Hi");

    env.interactWith(env.other_target);

    EXPECT_EQ(env.capture.shown_texts.size(), 1U);
    EXPECT_EQ(env.capture.hide_count, 1);
    EXPECT_TRUE(env.host.exec("assert(switched_done == true)"));
}

TEST(ScriptDialogueHelperTest, CompletionEventIsMarkedHandledToAvoidImmediateRestart) {
    ScriptDialogueHelperEnv env{};

    ASSERT_TRUE(env.host.exec(R"(
        local dialogue = tf.script.require("lib.dialogue")
        restart_guard_starts = 0
        restart_guard_done = nil
        restart_guard_active = false

        assert(tf.event.on("interact", function(evt)
            if evt.target == nil or evt.target.entity_id ~= test_target.entity_id then
                return
            end
            if evt.dialogue_handled or restart_guard_active then
                return
            end

            restart_guard_starts = restart_guard_starts + 1
            restart_guard_active = true
            assert(dialogue.start(evt.target, {"Hi", "Bye"}, function(interrupted)
                restart_guard_active = false
                restart_guard_done = interrupted
            end) == true)
        end) == true)
    )"));

    env.interact();
    env.interact();
    env.interact();

    EXPECT_EQ(env.capture.shown_texts.size(), 2U);
    EXPECT_EQ(env.capture.hide_count, 1);
    EXPECT_TRUE(env.host.exec(R"(
        assert(restart_guard_starts == 1)
        assert(restart_guard_done == false)
    )"));
}

} // namespace game::script
