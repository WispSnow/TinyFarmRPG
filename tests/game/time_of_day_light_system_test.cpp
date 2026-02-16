#include <gtest/gtest.h>

#include "engine/component/light_component.h"
#include "engine/component/tags.h"
#include "engine/utils/events.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/system/time_of_day_light_system.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::system {

TEST(TimeOfDayLightSystemTest, TogglesNightOnlyAndDayOnlyLights) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto& time = registry.ctx().emplace<game::data::GameTime>();

    const entt::entity night_light = registry.create();
    registry.emplace<engine::component::PointLightComponent>(night_light);
    registry.emplace<game::component::NightOnlyTag>(night_light);

    const entt::entity day_light = registry.create();
    registry.emplace<engine::component::PointLightComponent>(day_light);
    registry.emplace<game::component::DayOnlyTag>(day_light);

    const entt::entity conflicting = registry.create();
    registry.emplace<engine::component::PointLightComponent>(conflicting);
    registry.emplace<game::component::NightOnlyTag>(conflicting);
    registry.emplace<game::component::DayOnlyTag>(conflicting);

    TimeOfDayLightSystem system(registry, dispatcher);

    time.hour_ = 12.0f;
    time.minute_ = 0.0f;
    dispatcher.enqueue<engine::utils::HourChangedEvent>(time.day_, time.hour_);
    dispatcher.update();
    EXPECT_TRUE(registry.all_of<engine::component::LightDisabledTag>(night_light));
    EXPECT_FALSE(registry.all_of<engine::component::LightDisabledTag>(day_light));
    EXPECT_TRUE(registry.all_of<engine::component::LightDisabledTag>(conflicting));
    EXPECT_FALSE(registry.all_of<engine::component::InvisibleTag>(night_light));
    EXPECT_FALSE(registry.all_of<engine::component::InvisibleTag>(day_light));
    EXPECT_FALSE(registry.all_of<engine::component::InvisibleTag>(conflicting));

    time.hour_ = 19.0f;
    time.minute_ = 0.0f;
    dispatcher.enqueue<engine::utils::HourChangedEvent>(time.day_, time.hour_);
    dispatcher.update();
    EXPECT_FALSE(registry.all_of<engine::component::LightDisabledTag>(night_light));
    EXPECT_TRUE(registry.all_of<engine::component::LightDisabledTag>(day_light));
    EXPECT_TRUE(registry.all_of<engine::component::LightDisabledTag>(conflicting));
    EXPECT_FALSE(registry.all_of<engine::component::InvisibleTag>(night_light));
    EXPECT_FALSE(registry.all_of<engine::component::InvisibleTag>(day_light));
    EXPECT_FALSE(registry.all_of<engine::component::InvisibleTag>(conflicting));
}

} // namespace game::system
