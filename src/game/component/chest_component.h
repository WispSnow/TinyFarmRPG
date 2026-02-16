#pragma once

#include <entt/entity/entity.hpp>
#include <vector>

namespace game::component {

struct ItemReward {
    entt::id_type item_id_{entt::null};
    int count_{0};
};

struct ChestComponent {
    int chest_id_{0};
    bool opened_{false};
    std::vector<ItemReward> rewards_{};
};

} // namespace game::component
