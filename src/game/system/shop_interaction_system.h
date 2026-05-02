#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::core {
class Context;
}

namespace game::data {
class ItemCatalog;
class ShopCatalog;
struct ShopData;
}

namespace game::defs {
struct InteractCommand;
}

namespace game::component {
struct DialogueComponent;
}

namespace game::domain {
class ShopTransactionService;
}

namespace game::system {

class ShopInteractionSystem final {
    entt::registry& registry_;
    engine::core::Context& context_;
    entt::dispatcher& dispatcher_;
    const game::data::ShopCatalog& shop_catalog_;
    game::data::ItemCatalog& item_catalog_;
    game::domain::ShopTransactionService& shop_transaction_service_;

public:
    ShopInteractionSystem(entt::registry& registry,
                          engine::core::Context& context,
                          const game::data::ShopCatalog& shop_catalog,
                          game::data::ItemCatalog& item_catalog,
                          game::domain::ShopTransactionService& shop_transaction_service);
    ~ShopInteractionSystem();

    void update(float delta_time);

private:
    entt::entity active_merchant_{entt::null};

    void onInteractCommand(const game::defs::InteractCommand& event);
    void showMerchantGreeting(entt::entity merchant,
                              game::component::DialogueComponent& dialogue,
                              const game::data::ShopData& shop);
    [[nodiscard]] bool isMerchantGreetingInRange(entt::entity player,
                                                  entt::entity merchant,
                                                  const game::component::DialogueComponent& dialogue) const;
    void closeMerchantGreeting(entt::entity merchant);
    void openShopMenu(entt::entity player, const game::data::ShopData& shop);
};

} // namespace game::system
