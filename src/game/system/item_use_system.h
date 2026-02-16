#pragma once

#include "game/defs/events.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::data {
class ItemCatalog;
}

namespace game::system {

class ItemUseSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    game::data::ItemCatalog& catalog_;

    float notification_timer_{0.0f};
    entt::entity notification_target_{entt::null};

public:
    ItemUseSystem(entt::registry& registry, entt::dispatcher& dispatcher, game::data::ItemCatalog& catalog);
    ~ItemUseSystem();

    void update(float delta_time);

private:
    void onUseItem(const game::defs::UseItemRequest& evt);
    void updateNotification(float delta_time);
};

} // namespace game::system

