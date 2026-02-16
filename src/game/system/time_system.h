#pragma once

#include "game/data/game_time.h"
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::defs {
struct AdvanceTimeRequest;
}

namespace game::system {

/**
 * @brief 游戏时间系统
 * 
 * 负责更新游戏内时间，计算时段变化，并触发相应的时间事件。
 */
class TimeSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

public:
    explicit TimeSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~TimeSystem();

    /**
     * @brief 更新游戏时间
     * 
     * @param delta_time 帧间时间差（秒）
     */
    void update(float delta_time);

private:
    void subscribe();
    void unsubscribe();

    void onAdvanceTimeRequest(const game::defs::AdvanceTimeRequest& event);
    void advanceTimeHours(game::data::GameTime& time, int hours);
};

} // namespace game::system
