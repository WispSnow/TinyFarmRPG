#pragma once

#include "game/defs/crop_defs.h"
#include <cstdint>

namespace game::component {

/// @brief 作物组件，记录作物的生长状态
struct CropComponent {
    game::defs::CropType type_{game::defs::CropType::Unknown};           ///< 作物类型
    game::defs::GrowthStage current_stage_{game::defs::GrowthStage::Seed}; ///< 当前生长阶段
    std::uint32_t planted_day_{0};                                        ///< 种植时的天数
    int stage_countdown_{0};                                               ///< 当前阶段的剩余天数（倒计时），<=0时推进到下一阶段
    
    CropComponent() = default;
    
    /**
     * @brief 构造函数
     * @param type 作物类型
     * @param planted_day 种植时的天数
     * @param initial_countdown 初始阶段的倒计时天数
     */
    CropComponent(game::defs::CropType type, std::uint32_t planted_day, int initial_countdown)
        : type_(type), planted_day_(planted_day), stage_countdown_(initial_countdown) {}

};

} // namespace game::component

