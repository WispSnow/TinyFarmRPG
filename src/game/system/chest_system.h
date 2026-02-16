#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <string>

namespace engine::utils {
struct AnimationFinishedEvent;
}

namespace game::defs {
struct InteractRequest;
}

namespace game::data {
class ItemCatalog;
}

namespace game::world {
class WorldState;
}

namespace game::system {

class ChestSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    game::world::WorldState& world_state_;
    game::data::ItemCatalog& item_catalog_;

    float notification_timer_{0.0f};
    entt::entity notification_target_{entt::null};

public:
    ChestSystem(entt::registry& registry,
                entt::dispatcher& dispatcher,
                game::world::WorldState& world_state,
                game::data::ItemCatalog& item_catalog);
    ~ChestSystem();

    void update(float delta_time);

private:
    void updateNotification(float delta_time);
    void showNotification(entt::entity player, std::string text);
    void hideNotification(entt::entity player);

    void onInteractRequest(const game::defs::InteractRequest& event);
    bool tryOpenChest(entt::entity player, entt::entity chest_entity);
    void onAnimationFinished(const engine::utils::AnimationFinishedEvent& event);
};

} // namespace game::system
