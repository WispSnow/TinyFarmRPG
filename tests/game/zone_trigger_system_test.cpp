#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "game/component/script_zone_component.h"
#include "game/component/tags.h"
#include "game/defs/events_map.h"
#include "game/system/zone_trigger_system.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <string>
#include <vector>

namespace {

struct ZoneEventCapture {
    std::vector<game::defs::ZoneEnteredEvent> entered{};
    std::vector<game::defs::ZoneExitedEvent> exited{};

    void onEntered(const game::defs::ZoneEnteredEvent& event) {
        entered.push_back(event);
    }

    void onExited(const game::defs::ZoneExitedEvent& event) {
        exited.push_back(event);
    }
};

} // namespace

namespace game::system {
namespace {

TEST(ZoneTriggerSystemTest, EmitsEnterAndExitOnlyOnBoundaryChanges) {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::world::WorldState world_state{};
    const entt::id_type map_id = world_state.ensureExternalMap("home_exterior");
    world_state.setCurrentMap(map_id);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    auto& transform = registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});

    const entt::entity zone = registry.create();
    registry.emplace<game::component::ScriptZoneComponent>(
        zone,
        game::component::ScriptZoneComponent{
            .rect_ = engine::utils::Rect{glm::vec2{10.0F, 10.0F}, glm::vec2{20.0F, 20.0F}},
            .map_id_ = map_id,
            .zone_id_ = "zone.home.seed_hint",
            .zone_id_hash_ = entt::hashed_string{"zone.home.seed_hint"}.value(),
        });

    ZoneEventCapture capture{};
    dispatcher.sink<game::defs::ZoneEnteredEvent>().connect<&ZoneEventCapture::onEntered>(&capture);
    dispatcher.sink<game::defs::ZoneExitedEvent>().connect<&ZoneEventCapture::onExited>(&capture);

    ZoneTriggerSystem system{registry, dispatcher, world_state};
    system.update(0.016F);
    EXPECT_TRUE(capture.entered.empty());
    EXPECT_TRUE(capture.exited.empty());

    transform.position_ = {12.0F, 12.0F};
    system.update(0.016F);
    ASSERT_EQ(capture.entered.size(), 1U);
    EXPECT_EQ(capture.entered.front().player, player);
    EXPECT_EQ(capture.entered.front().zone, zone);
    EXPECT_EQ(capture.entered.front().zone_id, "zone.home.seed_hint");
    EXPECT_EQ(capture.entered.front().map_id, map_id);
    EXPECT_EQ(capture.entered.front().map_name, "home_exterior");

    system.update(0.016F);
    EXPECT_EQ(capture.entered.size(), 1U);
    EXPECT_TRUE(capture.exited.empty());

    transform.position_ = {40.0F, 40.0F};
    system.update(0.016F);
    ASSERT_EQ(capture.exited.size(), 1U);
    EXPECT_EQ(capture.exited.front().player, player);
    EXPECT_EQ(capture.exited.front().zone, zone);
    EXPECT_EQ(capture.exited.front().zone_id, "zone.home.seed_hint");

    system.update(0.016F);
    EXPECT_EQ(capture.entered.size(), 1U);
    EXPECT_EQ(capture.exited.size(), 1U);
}

TEST(ZoneTriggerSystemTest, FiltersZonesByCurrentMap) {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::world::WorldState world_state{};
    const entt::id_type home_id = world_state.ensureExternalMap("home_exterior");
    const entt::id_type town_id = world_state.ensureExternalMap("town");
    world_state.setCurrentMap(home_id);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{12.0F, 12.0F});

    const entt::entity zone = registry.create();
    registry.emplace<game::component::ScriptZoneComponent>(
        zone,
        game::component::ScriptZoneComponent{
            .rect_ = engine::utils::Rect{glm::vec2{10.0F, 10.0F}, glm::vec2{20.0F, 20.0F}},
            .map_id_ = town_id,
            .zone_id_ = "zone.town.notice",
            .zone_id_hash_ = entt::hashed_string{"zone.town.notice"}.value(),
        });

    ZoneEventCapture capture{};
    dispatcher.sink<game::defs::ZoneEnteredEvent>().connect<&ZoneEventCapture::onEntered>(&capture);

    ZoneTriggerSystem system{registry, dispatcher, world_state};
    system.update(0.016F);
    EXPECT_TRUE(capture.entered.empty());

    world_state.setCurrentMap(town_id);
    system.update(0.016F);
    ASSERT_EQ(capture.entered.size(), 1U);
    EXPECT_EQ(capture.entered.front().zone, zone);
    EXPECT_EQ(capture.entered.front().map_id, town_id);
    EXPECT_EQ(capture.entered.front().map_name, "town");
}

} // namespace
} // namespace game::system
