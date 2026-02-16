// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/component/light_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/defs/events.h"
#include "game/system/light_toggle_system.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <fstream>

using namespace entt::literals;

namespace game::system {

TEST(LightToggleSystemTest, TogglesPlayerLightWithIndoorOrDarkGate) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    auto& time = registry.ctx().emplace<game::data::GameTime>();
    time.day_ = 1;
    time.hour_ = 12.0f;
    time.minute_ = 0.0f;

    game::world::WorldState world_state{};
    const std::filesystem::path world_path = std::filesystem::path(PROJECT_SOURCE_DIR) / "assets/maps/farm-rpg.world";
    ASSERT_TRUE(world_state.loadFromWorldFile(world_path.string(), entt::null));

    const auto* home_exterior = world_state.getMapState("home_exterior");
    ASSERT_NE(home_exterior, nullptr);
    const entt::id_type home_interior_id = world_state.ensureExternalMap("home_interior");

    world_state.setCurrentMap(home_exterior->info.id);
    registry.ctx().emplace<game::world::WorldState*>(&world_state);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{100.0f, 200.0f});

    const std::filesystem::path tmp_cfg = std::filesystem::temp_directory_path() / "light_toggle_system_test.json";
    {
        std::ofstream out(tmp_cfg);
        out << R"json(
{
  "player_follow_light": {
    "enabled_by_default": false,
    "radius": 123.0,
    "intensity": 0.42,
    "color": { "r": 1.0, "g": 0.8, "b": 0.6 },
    "offset": { "x": 0.0, "y": -10.0 }
  }
}
)json";
    }

    LightToggleSystem system(registry, dispatcher, tmp_cfg.string());

    system.update();
    ASSERT_TRUE(registry.all_of<engine::component::PointLightComponent>(player));
    EXPECT_TRUE(registry.all_of<engine::component::LightDisabledTag>(player));

    auto& light = registry.get<engine::component::PointLightComponent>(player);
    EXPECT_FLOAT_EQ(light.radius, 123.0f);
    EXPECT_FLOAT_EQ(light.options.intensity, 0.42f);
    EXPECT_FLOAT_EQ(light.options.color.r, 1.0f);
    EXPECT_FLOAT_EQ(light.options.color.g, 0.8f);
    EXPECT_FLOAT_EQ(light.options.color.b, 0.6f);
    EXPECT_FLOAT_EQ(light.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(light.offset.y, -10.0f);

    dispatcher.trigger(game::defs::ToggleLightRequest{"player_follow_light"_hs});
    system.update();
    EXPECT_TRUE(registry.all_of<engine::component::LightDisabledTag>(player));

    time.hour_ = 19.0f;
    time.minute_ = 0.0f;
    system.update();
    EXPECT_FALSE(registry.all_of<engine::component::LightDisabledTag>(player));

    world_state.setCurrentMap(home_interior_id);
    time.hour_ = 12.0f;
    time.minute_ = 0.0f;
    system.update();
    EXPECT_FALSE(registry.all_of<engine::component::LightDisabledTag>(player));

    dispatcher.trigger(game::defs::ToggleLightRequest{"player_follow_light"_hs});
    system.update();
    EXPECT_TRUE(registry.all_of<engine::component::LightDisabledTag>(player));
}

} // namespace game::system
// NOLINTEND

