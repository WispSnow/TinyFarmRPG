#include <gtest/gtest.h>

#include "game/defs/events.h"
#include "game/data/game_time.h"
#include "game/system/time_system.h"

#include "engine/utils/events.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

namespace {

struct TimeAdvanceEvents {
    std::vector<std::pair<std::uint32_t, float>> hours{};
    std::vector<std::uint32_t> days{};
    std::vector<std::tuple<std::uint32_t, float, std::uint8_t>> periods{};

    void onHour(const engine::utils::HourChangedEvent& evt) { hours.emplace_back(evt.day_, evt.hour_); }
    void onDay(const engine::utils::DayChangedEvent& evt) { days.push_back(evt.day_); }
    void onPeriod(const engine::utils::TimeOfDayChangedEvent& evt) {
        periods.emplace_back(evt.day_, evt.hour_, evt.time_of_day_);
    }
};

} // namespace

namespace game::system {

TEST(TimeAdvanceSleepTest, AdvancesAcrossMidnight) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    auto& time = registry.ctx().get<game::data::GameTime>();
    time.day_ = 1;
    time.hour_ = 23.0f;
    time.minute_ = 0.0f;
    time.time_of_day_ = time.calculateTimeOfDay(time.hour_);

    TimeSystem time_system(registry, dispatcher);

    TimeAdvanceEvents events{};
    dispatcher.sink<engine::utils::HourChangedEvent>().connect<&TimeAdvanceEvents::onHour>(&events);
    dispatcher.sink<engine::utils::DayChangedEvent>().connect<&TimeAdvanceEvents::onDay>(&events);
    dispatcher.sink<engine::utils::TimeOfDayChangedEvent>().connect<&TimeAdvanceEvents::onPeriod>(&events);

    dispatcher.trigger(game::defs::AdvanceTimeRequest{2});
    dispatcher.update();

    EXPECT_EQ(time.day_, 2u);
    EXPECT_FLOAT_EQ(time.hour_, 1.0f);
    EXPECT_FLOAT_EQ(time.minute_, 0.0f);

    ASSERT_EQ(events.hours.size(), 2u);
    EXPECT_EQ(events.hours[0].first, 1u);
    EXPECT_FLOAT_EQ(events.hours[0].second, 24.0f);
    EXPECT_EQ(events.hours[1].first, 2u);
    EXPECT_FLOAT_EQ(events.hours[1].second, 1.0f);

    ASSERT_EQ(events.days.size(), 1u);
    EXPECT_EQ(events.days[0], 2u);
}

TEST(TimeAdvanceSleepTest, AdvancesFullDayKeepsClockTime) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    registry.ctx().emplace<game::data::GameTime>();
    auto& time = registry.ctx().get<game::data::GameTime>();
    time.day_ = 1;
    time.hour_ = 6.0f;
    time.minute_ = 30.0f;
    time.time_of_day_ = time.calculateTimeOfDay(time.hour_);

    TimeSystem time_system(registry, dispatcher);

    TimeAdvanceEvents events{};
    dispatcher.sink<engine::utils::DayChangedEvent>().connect<&TimeAdvanceEvents::onDay>(&events);

    dispatcher.trigger(game::defs::AdvanceTimeRequest{24});
    dispatcher.update();

    EXPECT_EQ(time.day_, 2u);
    EXPECT_FLOAT_EQ(time.hour_, 6.0f);
    EXPECT_FLOAT_EQ(time.minute_, 30.0f);

    ASSERT_EQ(events.days.size(), 1u);
    EXPECT_EQ(events.days[0], 2u);
}

} // namespace game::system
