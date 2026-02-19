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
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{16.0f, 24.0f});
    return player;
}

} // namespace

namespace game::script {

TEST(ScriptHostHandleLifecycleTest, StaleHandleAfterShutdownIsRejected) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    const entt::entity player = seedPlayer(registry);

    ScriptHost host(registry, dispatcher);
    ASSERT_TRUE(host.init());

    const ScriptEntityHandle handle = host.makeHandle(player);
    host.shutdown();

    entt::entity resolved = entt::null;
    EXPECT_FALSE(host.validateHandle(handle, resolved, "test.after_shutdown"));
    EXPECT_EQ(resolved, static_cast<entt::entity>(entt::null));
    EXPECT_FALSE(host.exec("assert(true)"));
}

TEST(ScriptHostHandleLifecycleTest, SceneSwitchInvalidatesOldHandles) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    const entt::entity player = seedPlayer(registry);

    ScriptHost host_a(registry, dispatcher);
    ASSERT_TRUE(host_a.init());
    const ScriptEntityHandle old_handle = host_a.makeHandle(player);
    const std::uint64_t old_scene_token = host_a.sceneToken();

    host_a.shutdown();

    ScriptHost host_b(registry, dispatcher);
    ASSERT_TRUE(host_b.init());
    EXPECT_NE(old_scene_token, host_b.sceneToken());

    entt::entity resolved = entt::null;
    EXPECT_FALSE(host_b.validateHandle(old_handle, resolved, "test.cross_scene"));
    EXPECT_EQ(resolved, static_cast<entt::entity>(entt::null));

    const ScriptEntityHandle new_handle = host_b.makeHandle(player);
    EXPECT_TRUE(host_b.validateHandle(new_handle, resolved, "test.new_scene"));
    EXPECT_EQ(resolved, player);
}

} // namespace game::script
