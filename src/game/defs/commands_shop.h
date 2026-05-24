#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

struct OpenShopCommand {
    entt::entity player{entt::null};
    entt::entity merchant{entt::null};
    entt::id_type shop_id_hash{entt::null};
    std::string shop_id{};
};

} // namespace game::defs
