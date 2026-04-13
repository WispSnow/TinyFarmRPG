#pragma once

#include "engine/debug/debug_panel.h"

#include <string>

#include <entt/entity/fwd.hpp>

namespace game::data {
class ItemCatalog;
class ShopCatalog;
}

namespace game::domain {
class ShopTransactionService;
}

namespace game::debug {

class ShopDebugPanel final : public engine::debug::DebugPanel {
    entt::registry& registry_;
    const game::data::ShopCatalog* shop_catalog_{nullptr};
    const game::data::ItemCatalog* item_catalog_{nullptr};
    game::domain::ShopTransactionService* shop_transaction_service_{nullptr};

    std::string selected_shop_id_{};
    int selected_buy_index_{0};
    int selected_sell_index_{0};
    int requested_buy_quantity_{1};
    int requested_sell_quantity_{1};
    bool show_unsellable_slots_{true};
    int gold_seed_step_{1000};
    std::string status_{};

public:
    ShopDebugPanel(entt::registry& registry,
                   const game::data::ShopCatalog* shop_catalog,
                   const game::data::ItemCatalog* item_catalog,
                   game::domain::ShopTransactionService* shop_transaction_service);

    std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug
