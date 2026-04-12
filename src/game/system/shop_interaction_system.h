#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::core {
class Context;
}

namespace game::data {
class ItemCatalog;
class ShopCatalog;
}

namespace game::defs {
struct InteractCommand;
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

private:
    void onInteractCommand(const game::defs::InteractCommand& event);
};

} // namespace game::system
