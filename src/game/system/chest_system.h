#pragma once

#include "system_helpers.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <string>

namespace engine::utils {
struct AnimationFinishedEvent;
}

namespace game::defs {
struct InteractCommand;
struct OpenScriptedChestCommand;
}

namespace game::data {
class ItemCatalog;
}

namespace game::domain {
class InventoryDomainService;
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
    game::domain::InventoryDomainService& inventory_domain_service_;

    game::system::helpers::NotificationTimer notification_{};

public:
    ChestSystem(entt::registry& registry,
                entt::dispatcher& dispatcher,
                game::world::WorldState& world_state,
                game::data::ItemCatalog& item_catalog,
                game::domain::InventoryDomainService& inventory_domain_service);
    ~ChestSystem();

    void update(float delta_time);

private:
    void updateNotification(float delta_time);
    void showNotification(entt::entity player, std::string text);

    void onInteractCommand(const game::defs::InteractCommand& event);
    void onOpenScriptedChestCommand(const game::defs::OpenScriptedChestCommand& event);
    bool tryOpenChest(entt::entity player, entt::entity chest_entity);
    bool tryOpenScriptedChest(entt::entity player, entt::entity chest_entity, std::string notification_text);
    bool openChest(entt::entity player, entt::entity chest_entity, std::string notification_text, bool grant_rewards);
    void onAnimationFinished(const engine::utils::AnimationFinishedEvent& event);
};

} // namespace game::system
