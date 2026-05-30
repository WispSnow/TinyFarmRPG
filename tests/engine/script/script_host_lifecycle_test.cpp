#include <gtest/gtest.h>

#include "engine/script/script_host.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

[[nodiscard]] std::filesystem::path makeReloadTestScriptPath(const void* owner) {
    const auto suffix = reinterpret_cast<std::uintptr_t>(owner);
    return std::filesystem::temp_directory_path() /
           ("tinyfarm_script_host_reload_" + std::to_string(suffix) + ".lua");
}

} // namespace

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

TEST(ScriptHostLifecycleTest, ReloadInvalidatesOldHandles) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    const entt::entity entity = registry.create();
    const std::filesystem::path script_path = makeReloadTestScriptPath(&registry);

    {
        std::ofstream script(script_path);
        ASSERT_TRUE(script.good());
        script << "reload_marker = (reload_marker or 0) + 1\n";
    }

    ScriptHost host(registry);
    ASSERT_TRUE(host.init(dispatcher));
    ASSERT_TRUE(host.loadFile(script_path.string()));

    const ScriptEntityHandle old_handle = host.makeHandle(entity);
    const std::uint64_t old_scene_token = host.sceneToken();

    entt::entity resolved = entt::null;
    ASSERT_TRUE(host.validateHandle(old_handle, resolved, "test.before_reload"));
    EXPECT_EQ(resolved, entity);

    ASSERT_TRUE(host.reload());
    EXPECT_NE(old_scene_token, host.sceneToken());

    EXPECT_FALSE(host.validateHandle(old_handle, resolved, "test.after_reload"));
    EXPECT_EQ(resolved, static_cast<entt::entity>(entt::null));

    const ScriptEntityHandle new_handle = host.makeHandle(entity);
    EXPECT_TRUE(host.validateHandle(new_handle, resolved, "test.after_reload_new_handle"));
    EXPECT_EQ(resolved, entity);
    EXPECT_TRUE(host.exec("assert(reload_marker == 2)"));

    std::error_code ignored;
    std::filesystem::remove(script_path, ignored);
}

} // namespace engine::script
