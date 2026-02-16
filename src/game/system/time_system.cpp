#include "time_system.h"
#include "game/data/game_time.h"
#include "game/defs/events.h"
#include "engine/utils/events.h"
#include <spdlog/spdlog.h>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::system {

TimeSystem::TimeSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    subscribe();
}

TimeSystem::~TimeSystem() {
    unsubscribe();
}

void TimeSystem::subscribe() {
    dispatcher_.sink<game::defs::AdvanceTimeRequest>().connect<&TimeSystem::onAdvanceTimeRequest>(this);
}

void TimeSystem::unsubscribe() {
    dispatcher_.sink<game::defs::AdvanceTimeRequest>().disconnect<&TimeSystem::onAdvanceTimeRequest>(this);
}

void TimeSystem::update(float delta_time) {
    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        spdlog::warn("TimeSystem::update: 注册表上下文中未找到 GameTime");
        return;
    }

    // 如果暂停，直接返回
    if (game_time->paused_) {
        return;
    }

    // 计算时间流逝
    float scaled_delta = delta_time * game_time->time_scale_;
    float minutes_to_add = scaled_delta * game_time->config_.minutes_per_real_second_;
    game_time->minute_ += minutes_to_add;

    // 处理分钟溢出
    while (game_time->minute_ >= 60.0f) {
        game_time->minute_ -= 60.0f;
        game_time->hour_ += 1.0f;

        // 触发小时变化事件
        dispatcher_.enqueue<engine::utils::HourChangedEvent>(game_time->day_, game_time->hour_);
        spdlog::trace("小时变化：Day {}, {:.2f}:00", game_time->day_, game_time->hour_);

        // 检查是否跨天
        if (game_time->hour_ >= 24.0f) {
            game_time->hour_ -= 24.0f;
            game_time->day_++;
            dispatcher_.enqueue<engine::utils::DayChangedEvent>(game_time->day_);
            spdlog::info("新的一天开始：Day {}", game_time->day_);
        }
    }

    // 检查时段变化
    game::data::TimeOfDay new_time_of_day = game_time->calculateTimeOfDay(game_time->hour_);
    if (new_time_of_day != game_time->time_of_day_) {
        game_time->time_of_day_ = new_time_of_day;
        dispatcher_.enqueue<engine::utils::TimeOfDayChangedEvent>(game_time->day_,
            game_time->hour_,
            static_cast<std::uint8_t>(new_time_of_day));
        spdlog::info("时段变化：Day {}, {:.2f}:00, 时间段：{}", game_time->day_, game_time->hour_, static_cast<std::uint8_t>(new_time_of_day));
    }

}

void TimeSystem::onAdvanceTimeRequest(const game::defs::AdvanceTimeRequest& event) {
    if (event.hours <= 0) {
        return;
    }

    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        spdlog::warn("TimeSystem: GameTime not found in registry context; advance time request ignored");
        return;
    }

    advanceTimeHours(*game_time, event.hours);
}

void TimeSystem::advanceTimeHours(game::data::GameTime& time, int hours) {
    if (hours <= 0) {
        return;
    }

    for (int i = 0; i < hours; ++i) {
        time.hour_ += 1.0f;
        dispatcher_.enqueue<engine::utils::HourChangedEvent>(time.day_, time.hour_);

        if (time.hour_ >= 24.0f) {
            time.hour_ -= 24.0f;
            ++time.day_;
            dispatcher_.enqueue<engine::utils::DayChangedEvent>(time.day_);
        }

        const auto new_tod = time.calculateTimeOfDay(time.hour_);
        if (new_tod != time.time_of_day_) {
            time.time_of_day_ = new_tod;
            dispatcher_.enqueue<engine::utils::TimeOfDayChangedEvent>(
                time.day_, time.hour_, static_cast<std::uint8_t>(new_tod));
        }
    }
}

} // namespace game::system
