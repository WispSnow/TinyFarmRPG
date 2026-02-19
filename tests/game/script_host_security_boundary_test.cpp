#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/script/script_host.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace {

entt::entity seedPlayer(entt::registry& registry) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{32.0f, 48.0f});
    return player;
}

} // namespace

namespace game::script {

TEST(ScriptHostSecurityBoundaryTest, OnlyWhitelistedApiIsExposed) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    (void)seedPlayer(registry);

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());

    EXPECT_TRUE(host.exec(R"(
        assert(tf ~= nil)
        assert(tf.time ~= nil)
        assert(tf.player ~= nil)
        assert(tf.command ~= nil)
        assert(tf.dialogue ~= nil)
        assert(io == nil)
        assert(os == nil)
        assert(package == nil)
        assert(rawset == nil)
        assert(rawget == nil)
    )"));
}

TEST(ScriptHostSecurityBoundaryTest, DofileLoadfileLoadAreBlocked) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    (void)seedPlayer(registry);

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());

    EXPECT_TRUE(host.exec(R"(
        assert(dofile == nil)
        assert(loadfile == nil)
        assert(load == nil)
    )"));
}

TEST(ScriptHostSecurityBoundaryTest, TfNamespaceIsReadOnly) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    (void)seedPlayer(registry);

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());

    EXPECT_TRUE(host.exec(R"(
        local ok1 = pcall(function()
            tf.new_field = 1
        end)
        assert(ok1 == false)

        local ok2 = pcall(function()
            tf.time.day = function() return 99 end
        end)
        assert(ok2 == false)
    )"));
}

TEST(ScriptHostSecurityBoundaryTest, InvalidHandleIsSafelyRejected) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    const entt::entity player = seedPlayer(registry);

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());

    const ScriptEntityHandle stale_handle = host.makeHandle(player);
    registry.destroy(player);

    entt::entity resolved = entt::null;
    EXPECT_FALSE(host.validateHandle(stale_handle, resolved, "test.invalid_handle"));
    EXPECT_EQ(resolved, static_cast<entt::entity>(entt::null));
}

TEST(ScriptHostSecurityBoundaryTest, InfiniteLoopIsAbortedByInstructionLimit) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    (void)seedPlayer(registry);

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());

    EXPECT_FALSE(host.exec("while true do end"));
    EXPECT_TRUE(host.exec("assert(1 + 1 == 2)"));
}

} // namespace game::script
