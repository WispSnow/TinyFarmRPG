#include <gtest/gtest.h>

#include "engine/script/script_binding_utils.h"
#include "engine/script/script_host.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <vector>

namespace {

void installReadOnlyTestModule(sol::state& lua,
                               engine::script::ScriptHost&,
                               entt::registry&,
                               entt::dispatcher&) {
    sol::table sub_impl = lua.create_table();
    sub_impl["value"] = 7;

    sol::table test_impl = lua.create_table();
    test_impl["sub"] = engine::script::createReadOnlyProxy(lua, sub_impl, "test.sub");
    test_impl.set_function("ping", []() -> int { return 42; });

    lua["test"] = engine::script::createReadOnlyProxy(lua, test_impl, "test");
}

std::vector<engine::script::ScriptModuleInstaller> readOnlyInstallers() {
    return {
        installReadOnlyTestModule,
    };
}

} // namespace

namespace engine::script {

TEST(ScriptHostSecurityTest, OnlyWhitelistedGlobalApiIsExposed) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    ScriptHost host(registry);
    ASSERT_TRUE(host.init(dispatcher));

    EXPECT_TRUE(host.exec(R"(
        assert(io == nil)
        assert(os == nil)
        assert(package == nil)
        assert(rawset == nil)
        assert(rawget == nil)
        assert(collectgarbage == nil)
    )"));
}

TEST(ScriptHostSecurityTest, DofileLoadfileLoadAreBlocked) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    ScriptHost host(registry);
    ASSERT_TRUE(host.init(dispatcher));

    EXPECT_TRUE(host.exec(R"(
        assert(dofile == nil)
        assert(loadfile == nil)
        assert(load == nil)
    )"));
}

TEST(ScriptHostSecurityTest, InstalledNamespaceCanBeReadOnly) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    ScriptHost host(registry);
    ASSERT_TRUE(host.init(dispatcher, readOnlyInstallers()));

    EXPECT_TRUE(host.exec(R"(
        assert(test ~= nil)
        assert(test.ping() == 42)
        assert(test.sub.value == 7)

        local ok1 = pcall(function()
            test.new_field = 1
        end)
        assert(ok1 == false)

        local ok2 = pcall(function()
            test.sub.value = 9
        end)
        assert(ok2 == false)
    )"));
}

TEST(ScriptHostSecurityTest, InvalidHandleIsSafelyRejected) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity entity = registry.create();

    ScriptHost host(registry);
    ASSERT_TRUE(host.init(dispatcher));

    const ScriptEntityHandle stale_handle = host.makeHandle(entity);
    registry.destroy(entity);

    entt::entity resolved = entt::null;
    EXPECT_FALSE(host.validateHandle(stale_handle, resolved, "test.invalid_handle"));
    EXPECT_EQ(resolved, static_cast<entt::entity>(entt::null));
}

TEST(ScriptHostSecurityTest, InfiniteLoopIsAbortedByInstructionLimit) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    ScriptHost host(registry);
    ASSERT_TRUE(host.init(dispatcher));

    EXPECT_FALSE(host.exec("while true do end"));
    EXPECT_TRUE(host.exec("assert(1 + 1 == 2)"));
}

} // namespace engine::script
