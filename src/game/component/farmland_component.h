#pragma once
#
#include <entt/entity/entity.hpp>
#include <entt/core/enum.hpp>

namespace game::component {

struct FarmlandComponent {
    enum class State {
        Normal = 0x00,
        Farmable = 0x01,
        Watered = 0x02,
        Cropped = 0x04,
        Harvested = 0x08,
        // 未来可补充更多属性
        _entt_enum_as_bitmask   ///< @brief 启用位掩码支持
    };
    State state_{State::Normal};
};

} // namespace game::component