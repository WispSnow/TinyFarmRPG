#pragma once

#include <string>

#include <entt/entity/entity.hpp>

namespace game::component {

struct MerchantComponent {
    std::string shop_id_{};
    entt::id_type shop_id_hash_{entt::null};
};

} // namespace game::component
