#include <gtest/gtest.h>

#include "engine/script/script_host.h"
#include "game/script/script_state.h"
#include "script_test_utils.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <string>
#include <variant>

namespace game::script {

TEST(ScriptStateTest, LuaApiStoresOnlyJsonCompatiblePrimitives) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    engine::script::ScriptHost host(registry);
    ASSERT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));

    EXPECT_TRUE(host.exec(R"(
        assert(tf.state.set("quest.first_delivery.stage", 2) == true)
        assert(tf.state.get_int("quest.first_delivery.stage", 0) == 2)
        assert(tf.state.get_number("quest.first_delivery.stage", 0.0) == 2.0)

        assert(tf.state.set("npc.lyria.mood", "happy") == true)
        assert(tf.state.get_string("npc.lyria.mood", "neutral") == "happy")

        assert(tf.state.set("flags.met_lyria", true) == true)
        assert(tf.state.get_bool("flags.met_lyria", false) == true)

        assert(tf.state.set("nullable.marker", nil) == true)
        assert(tf.state.has("nullable.marker") == true)
        assert(tf.state.get("nullable.marker", "fallback") == nil)

        assert(tf.state.get("missing.value", "fallback") == "fallback")
        assert(tf.state.get_int("missing.int", 7) == 7)
        assert(tf.state.add("quest.first_delivery.stage", 3) == 5)
        assert(tf.state.get_int("quest.first_delivery.stage", 0) == 5)

        assert(tf.state.set("", 1) == false)
        assert(tf.state.set("bad.table", {}) == false)
        assert(tf.state.add("npc.lyria.mood", 1) == nil)
        assert(tf.state.unset("npc.lyria.mood") == true)
        assert(tf.state.has("npc.lyria.mood") == false)
    )"));

    const auto* state = registry.ctx().find<game::script::ScriptStateStore>();
    ASSERT_NE(state, nullptr);

    const auto* stage = state->find("quest.first_delivery.stage");
    ASSERT_NE(stage, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(*stage));
    EXPECT_DOUBLE_EQ(std::get<double>(*stage), 5.0);

    const auto* met_lyria = state->find("flags.met_lyria");
    ASSERT_NE(met_lyria, nullptr);
    ASSERT_TRUE(std::holds_alternative<bool>(*met_lyria));
    EXPECT_TRUE(std::get<bool>(*met_lyria));

    const auto* nullable = state->find("nullable.marker");
    ASSERT_NE(nullable, nullptr);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(*nullable));
    EXPECT_EQ(state->find("bad.table"), nullptr);
}

} // namespace game::script
