#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/script/script_host.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string testCommandScriptPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/scripts/test_command.lua").string();
}

} // namespace

namespace game::script {

TEST(ScriptHostSmokeTest, LoadAndRunInlineScriptWithoutCrash) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    auto& game_time = registry.ctx().emplace<game::data::GameTime>();
    game_time.day_ = 3;
    game_time.hour_ = 7.0f;
    game_time.minute_ = 15.0f;

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{32.0f, 48.0f});

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());

    EXPECT_TRUE(host.exec(R"(
        assert(tf.player.exists() == true)
        assert(tf.time.day() == 3)
        assert(tf.time.hour() == 7)
        assert(tf.time.minute() == 15)
    )"));

    EXPECT_TRUE(host.exec("assert(tf.command.interact(0xffffffff) == false)"));
}

TEST(ScriptHostSmokeTest, LoadAndRunFileWithoutCrash) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    registry.ctx().emplace<game::data::GameTime>();
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0f, 0.0f});

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());
    ASSERT_TRUE(host.loadFile(testCommandScriptPath()));
    EXPECT_TRUE(host.exec("assert(type(issue_add_item) == 'function')"));
}

} // namespace game::script
