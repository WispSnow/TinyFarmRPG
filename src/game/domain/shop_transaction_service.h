#pragma once

#include <cstdint>
#include <string_view>

#include <entt/entity/entity.hpp>

namespace game::data {
class ItemCatalog;
class ShopCatalog;
}

namespace game::domain {

class InventoryDomainService;

enum class ShopTradeFailureReason : std::uint8_t {
    None = 0,
    InvalidPlayer,
    InvalidShop,
    InvalidItem,
    InvalidQuantity,
    ItemNotSoldHere,
    ItemNotSellable,
    InsufficientGold,
    InventoryFull,
    SlotMismatch,
    InsufficientItemCount
};

struct ShopBuyPreview {
    int requested_quantity{0};
    int resolved_quantity{0};
    int unit_price{0};
    int total_price{0};
    int final_gold_after{0};
    bool can_afford{false};
    bool has_space{false};
    ShopTradeFailureReason failure_reason{ShopTradeFailureReason::None};

    [[nodiscard]] bool canCommit() const {
        return failure_reason == ShopTradeFailureReason::None && can_afford && has_space;
    }
};

struct ShopSellPreview {
    int requested_quantity{0};
    int resolved_quantity{0};
    int unit_price{0};
    int total_price{0};
    int final_gold_after{0};
    ShopTradeFailureReason failure_reason{ShopTradeFailureReason::None};

    [[nodiscard]] bool canCommit() const {
        return failure_reason == ShopTradeFailureReason::None;
    }
};

struct ShopBuyResult {
    int requested_quantity{0};
    int resolved_quantity{0};
    int unit_price{0};
    int total_price{0};
    int final_gold_after{0};
    ShopTradeFailureReason failure_reason{ShopTradeFailureReason::None};

    [[nodiscard]] bool completed() const {
        return failure_reason == ShopTradeFailureReason::None;
    }
};

struct ShopSellResult {
    int requested_quantity{0};
    int resolved_quantity{0};
    int unit_price{0};
    int total_price{0};
    int final_gold_after{0};
    ShopTradeFailureReason failure_reason{ShopTradeFailureReason::None};

    [[nodiscard]] bool completed() const {
        return failure_reason == ShopTradeFailureReason::None;
    }
};

class ShopTransactionService final {
public:
    ShopTransactionService(entt::registry& registry,
                           game::data::ItemCatalog& item_catalog,
                           game::data::ShopCatalog& shop_catalog,
                           game::domain::InventoryDomainService& inventory_domain_service);

    [[nodiscard]] ShopBuyPreview previewBuy(entt::entity player,
                                            std::string_view shop_id,
                                            entt::id_type item_id,
                                            int quantity) const;
    [[nodiscard]] ShopBuyResult commitBuy(entt::entity player,
                                          std::string_view shop_id,
                                          entt::id_type item_id,
                                          int quantity) const;

    [[nodiscard]] ShopSellPreview previewSell(entt::entity player,
                                              entt::id_type item_id,
                                              int quantity,
                                              int slot_index) const;
    [[nodiscard]] ShopSellResult commitSell(entt::entity player,
                                            entt::id_type item_id,
                                            int quantity,
                                            int slot_index) const;

private:
    entt::registry& registry_;
    game::data::ItemCatalog& item_catalog_;
    game::data::ShopCatalog& shop_catalog_;
    game::domain::InventoryDomainService& inventory_domain_service_;
};

} // namespace game::domain
