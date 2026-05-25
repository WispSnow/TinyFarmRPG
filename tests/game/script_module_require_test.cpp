#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "engine/script/script_host.h"
#include "game/component/player_wallet_component.h"
#include "game/component/tags.h"
#include "game/defs/commands_interaction.h"
#include "game/script/script_event_bridge.h"
#include "game/script/script_state.h"
#include "script_test_utils.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>
#include <variant>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string moduleScriptRoot() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/scripts/modules").string();
}

[[nodiscard]] std::string reloadBootstrapPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/scripts/modules/reload_bootstrap.lua").string();
}

[[nodiscard]] std::string projectScriptRoot() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "scripts").string();
}

[[nodiscard]] std::string projectBootstrapPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "scripts/bootstrap.lua").string();
}

} // namespace

namespace game::script {

TEST(ScriptModuleRequireTest, RequireLoadsCachesAndRejectsInvalidNames) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    engine::script::ScriptHost host(registry);
    host.setScriptRoot(moduleScriptRoot());
    ASSERT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));

    EXPECT_TRUE(host.exec(R"(
        module_counter_loads = 0

        local counter = tf.script.require("lib.counter")
        assert(counter ~= nil)
        assert(counter.name == "counter")
        assert(counter.loads == 1)

        local again = tf.script.require("lib.counter")
        assert(again == counter)
        assert(module_counter_loads == 1)

        local uses = tf.script.require("lib.uses_event")
        assert(uses ~= nil)
        assert(uses.event_loaded == true)

        assert(tf.script.require("") == nil)
        assert(tf.script.require("lib..counter") == nil)
        assert(tf.script.require("../counter") == nil)
        assert(tf.script.require("lib.missing") == nil)
        assert(tf.script.require("lib.failure") == nil)
        assert(tf.script.require("lib.syntax_bad") == nil)
    )"));
}

TEST(ScriptModuleRequireTest, ProjectBootstrapLoadsModuleTree) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    engine::script::ScriptHost host(registry);
    host.setScriptRoot(projectScriptRoot());
    ASSERT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));

    EXPECT_TRUE(host.loadFile(projectBootstrapPath()));
}

TEST(ScriptModuleRequireTest, ProjectBootstrapSeedsInitialGoldOnce) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0f, 0.0f});
    registry.emplace<game::component::PlayerWalletComponent>(player);

    engine::script::ScriptHost host(registry);
    host.setScriptRoot(projectScriptRoot());
    ASSERT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));

    ASSERT_TRUE(host.loadFile(projectBootstrapPath()));
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 300);

    const auto* script_state = registry.ctx().find<game::script::ScriptStateStore>();
    ASSERT_NE(script_state, nullptr);
    const auto* seeded = script_state->find("player.initial_gold_300_seeded");
    ASSERT_NE(seeded, nullptr);
    ASSERT_TRUE(std::holds_alternative<bool>(*seeded));
    EXPECT_TRUE(std::get<bool>(*seeded));

    registry.get<game::component::PlayerWalletComponent>(player).gold_ = 42;
    ASSERT_TRUE(host.loadFile(projectBootstrapPath()));
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 42);
}

TEST(ScriptModuleRequireTest, ReloadClearsModuleCacheAndPreviouslyRegisteredCallbacks) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    engine::script::ScriptHost host(registry);
    host.setScriptRoot(moduleScriptRoot());
    ASSERT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));

    game::script::ScriptEventBridge bridge(host, registry, dispatcher);

    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0f, 0.0f});

    const entt::entity target = registry.create();
    registry.emplace<engine::component::TransformComponent>(target, glm::vec2{16.0f, 0.0f});

    ASSERT_TRUE(host.loadFile(reloadBootstrapPath()));

    dispatcher.trigger(game::defs::InteractCommand{player, target});
    EXPECT_TRUE(host.exec(R"(
        assert(reload_callback_hits == 1)
        assert(reload_module_loads == 1)
    )"));

    ASSERT_TRUE(host.reload());

    dispatcher.trigger(game::defs::InteractCommand{player, target});
    EXPECT_TRUE(host.exec(R"(
        assert(reload_callback_hits == 2)
        assert(reload_module_loads == 2)
    )"));
}

} // namespace game::script
