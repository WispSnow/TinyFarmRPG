#include <gtest/gtest.h>

#include "engine/script/script_host.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace engine::script {

TEST(ScriptHostLifecycleTest, StaleHandleAfterShutdownIsRejected) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    const entt::entity entity = registry.create();

    ScriptHost host(registry);
    ASSERT_TRUE(host.init(dispatcher));

    const ScriptEntityHandle handle = host.makeHandle(entity);
    host.shutdown();

    entt::entity resolved = entt::null;
    EXPECT_FALSE(host.validateHandle(handle, resolved, "test.after_shutdown"));
    EXPECT_EQ(resolved, static_cast<entt::entity>(entt::null));
    EXPECT_FALSE(host.exec("assert(true)"));
}

TEST(ScriptHostLifecycleTest, SceneSwitchInvalidatesOldHandles) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    const entt::entity entity = registry.create();

    ScriptHost host_a(registry);
    ASSERT_TRUE(host_a.init(dispatcher));
    const ScriptEntityHandle old_handle = host_a.makeHandle(entity);
    const std::uint64_t old_scene_token = host_a.sceneToken();

    host_a.shutdown();

    ScriptHost host_b(registry);
    ASSERT_TRUE(host_b.init(dispatcher));
    EXPECT_NE(old_scene_token, host_b.sceneToken());

    entt::entity resolved = entt::null;
    EXPECT_FALSE(host_b.validateHandle(old_handle, resolved, "test.cross_scene"));
    EXPECT_EQ(resolved, static_cast<entt::entity>(entt::null));

    const ScriptEntityHandle new_handle = host_b.makeHandle(entity);
    EXPECT_TRUE(host_b.validateHandle(new_handle, resolved, "test.new_scene"));
    EXPECT_EQ(resolved, entity);
}

} // namespace engine::script
