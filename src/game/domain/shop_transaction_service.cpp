#include "game/domain/shop_transaction_service.h"

#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/domain/inventory_domain_service.h"
#include "game/system/inventory_helpers.h"

#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <limits>
#include <vector>

namespace game::domain {
namespace {

template <typename TPreviewOrResult>
void fillTradeFields(TPreviewOrResult& out,
                     const int requested_quantity,
                     const int resolved_quantity,
                     const int unit_price,
                     const int total_price,
                     const int final_gold_after,
                     const ShopTradeFailureReason failure_reason) {
    out.requested_quantity = requested_quantity;
    out.resolved_quantity = resolved_quantity;
    out.unit_price = unit_price;
    out.total_price = total_price;
    out.final_gold_after = final_gold_after;
    out.failure_reason = failure_reason;
}

[[nodiscard]] bool isQuantityAllowedForItem(const game::data::ItemData& item, const int quantity) {
    if (quantity <= 0) {
        return false;
    }
    if (item.stack_limit_ <= 1 && quantity > 1) {
        return false;
    }
    return true;
}

[[nodiscard]] bool checkedMultiplyNonNegative(const int lhs, const int rhs, int& out) {
    if (lhs < 0 || rhs < 0) {
        return false;
    }
    if (lhs != 0 && rhs > std::numeric_limits<int>::max() / lhs) {
        return false;
    }

    out = lhs * rhs;
    return true;
}

[[nodiscard]] bool checkedAddNonNegative(const int lhs, const int rhs, int& out) {
    if (rhs < 0 || lhs > std::numeric_limits<int>::max() - rhs) {
        return false;
    }

    out = lhs + rhs;
    return true;
}

[[nodiscard]] bool checkedSubtractNonNegative(const int lhs, const int rhs, int& out) {
    if (rhs < 0 || lhs < std::numeric_limits<int>::min() + rhs) {
        return false;
    }

    out = lhs - rhs;
    return true;
}

[[nodiscard]] ShopBuyResult makeBuyResultFromFailure(const ShopBuyPreview& preview, const int current_gold) {
    ShopBuyResult result{};
    fillTradeFields(
        result,
        preview.requested_quantity,
        preview.resolved_quantity,
        preview.unit_price,
        preview.total_price,
        current_gold,
        preview.failure_reason);
    return result;
}

[[nodiscard]] ShopSellResult makeSellResultFromFailure(const ShopSellPreview& preview, const int current_gold) {
    ShopSellResult result{};
    fillTradeFields(
        result,
        preview.requested_quantity,
        preview.resolved_quantity,
        preview.unit_price,
        preview.total_price,
        current_gold,
        preview.failure_reason);
    return result;
}

} // namespace

ShopTransactionService::ShopTransactionService(entt::registry& registry,
                                               game::data::ItemCatalog& item_catalog,
                                               game::data::ShopCatalog& shop_catalog,
                                               game::domain::InventoryDomainService& inventory_domain_service)
    : registry_(registry),
      item_catalog_(item_catalog),
      shop_catalog_(shop_catalog),
      inventory_domain_service_(inventory_domain_service) {
}

ShopBuyPreview ShopTransactionService::previewBuy(const entt::entity player,
                                                  const std::string_view shop_id,
                                                  const entt::id_type item_id,
                                                  const int quantity) const {
    ShopBuyPreview preview{};
    preview.requested_quantity = quantity;

    if (player == entt::null || !registry_.valid(player)) {
        preview.failure_reason = ShopTradeFailureReason::InvalidPlayer;
        return preview;
    }

    const auto* wallet = registry_.try_get<game::component::PlayerWalletComponent>(player);
    const auto* inventory = registry_.try_get<game::component::InventoryComponent>(player);
    if (wallet == nullptr || inventory == nullptr) {
        preview.failure_reason = ShopTradeFailureReason::InvalidPlayer;
        preview.final_gold_after = wallet ? wallet->gold_ : 0;
        return preview;
    }

    preview.final_gold_after = wallet->gold_;

    if (quantity <= 0) {
        preview.failure_reason = ShopTradeFailureReason::InvalidQuantity;
        return preview;
    }

    const auto* shop = shop_catalog_.findShop(shop_id);
    if (shop == nullptr) {
        preview.failure_reason = ShopTradeFailureReason::InvalidShop;
        return preview;
    }

    const auto* item = item_catalog_.findItem(item_id);
    if (item == nullptr) {
        preview.failure_reason = ShopTradeFailureReason::InvalidItem;
        return preview;
    }

    if (!isQuantityAllowedForItem(*item, quantity)) {
        preview.failure_reason = ShopTradeFailureReason::InvalidQuantity;
        return preview;
    }

    const auto* buy_entry = shop_catalog_.findBuyEntry(shop->id_hash_, item_id);
    if (buy_entry == nullptr) {
        preview.failure_reason = ShopTradeFailureReason::ItemNotSoldHere;
        return preview;
    }

    preview.resolved_quantity = quantity;
    preview.unit_price = buy_entry->buy_price_;
    if (!checkedMultiplyNonNegative(preview.unit_price, preview.resolved_quantity, preview.total_price)) {
        preview.failure_reason = ShopTradeFailureReason::InvalidQuantity;
        return preview;
    }

    preview.can_afford = wallet->gold_ >= preview.total_price;
    if (!preview.can_afford) {
        static_cast<void>(checkedSubtractNonNegative(wallet->gold_, preview.total_price, preview.final_gold_after));
        preview.failure_reason = ShopTradeFailureReason::InsufficientGold;
        return preview;
    }
    if (!checkedSubtractNonNegative(wallet->gold_, preview.total_price, preview.final_gold_after)) {
        preview.failure_reason = ShopTradeFailureReason::InvalidQuantity;
        return preview;
    }

    std::vector<game::component::ItemStack> simulated_slots = inventory->slots_;
    const int stack_limit = game::system::detail::stackLimitOrDefault(&item_catalog_, item_id);
    preview.has_space =
        game::system::detail::simulateAdd(simulated_slots, -1, item_id, preview.resolved_quantity, stack_limit);
    if (!preview.has_space) {
        preview.failure_reason = ShopTradeFailureReason::InventoryFull;
        return preview;
    }

    preview.failure_reason = ShopTradeFailureReason::None;
    return preview;
}

ShopBuyResult ShopTransactionService::commitBuy(const entt::entity player,
                                                const std::string_view shop_id,
                                                const entt::id_type item_id,
                                                const int quantity) const {
    const ShopBuyPreview preview = previewBuy(player, shop_id, item_id, quantity);
    const auto* wallet = (player != entt::null && registry_.valid(player))
                             ? registry_.try_get<game::component::PlayerWalletComponent>(player)
                             : nullptr;
    const int current_gold = wallet ? wallet->gold_ : 0;
    if (!preview.canCommit()) {
        return makeBuyResultFromFailure(preview, current_gold);
    }

    auto* mutable_wallet = registry_.try_get<game::component::PlayerWalletComponent>(player);
    if (mutable_wallet == nullptr) {
        ShopBuyPreview failed_preview = preview;
        failed_preview.failure_reason = ShopTradeFailureReason::InvalidPlayer;
        return makeBuyResultFromFailure(failed_preview, 0);
    }

    const std::array<InventoryItemGrant, 1> grants{InventoryItemGrant{
        .item_id = item_id,
        .count = preview.resolved_quantity,
    }};
    const auto mutation = inventory_domain_service_.addItemsAtomically(player, grants);
    if (mutation.accepted != preview.resolved_quantity || mutation.rejected != 0) {
        spdlog::warn("ShopTransactionService: buy commit 结果异常 item_id={}, requested={}, accepted={}, rejected={}",
                     item_id,
                     preview.resolved_quantity,
                     mutation.accepted,
                     mutation.rejected);
        ShopBuyPreview failed_preview = preview;
        failed_preview.failure_reason = ShopTradeFailureReason::InventoryFull;
        return makeBuyResultFromFailure(failed_preview, mutable_wallet->gold_);
    }

    mutable_wallet->gold_ -= preview.total_price;

    ShopBuyResult result{};
    fillTradeFields(
        result,
        preview.requested_quantity,
        preview.resolved_quantity,
        preview.unit_price,
        preview.total_price,
        mutable_wallet->gold_,
        ShopTradeFailureReason::None);
    return result;
}

ShopSellPreview ShopTransactionService::previewSell(const entt::entity player,
                                                    const entt::id_type item_id,
                                                    const int quantity,
                                                    const int slot_index) const {
    ShopSellPreview preview{};
    preview.requested_quantity = quantity;

    if (player == entt::null || !registry_.valid(player)) {
        preview.failure_reason = ShopTradeFailureReason::InvalidPlayer;
        return preview;
    }

    const auto* wallet = registry_.try_get<game::component::PlayerWalletComponent>(player);
    const auto* inventory = registry_.try_get<game::component::InventoryComponent>(player);
    if (wallet == nullptr || inventory == nullptr) {
        preview.failure_reason = ShopTradeFailureReason::InvalidPlayer;
        preview.final_gold_after = wallet ? wallet->gold_ : 0;
        return preview;
    }

    preview.final_gold_after = wallet->gold_;

    if (quantity <= 0) {
        preview.failure_reason = ShopTradeFailureReason::InvalidQuantity;
        return preview;
    }

    const auto* item = item_catalog_.findItem(item_id);
    if (item == nullptr) {
        preview.failure_reason = ShopTradeFailureReason::InvalidItem;
        return preview;
    }

    const auto* sell_rule = shop_catalog_.findSellRule(item_id);
    if (sell_rule == nullptr) {
        preview.failure_reason = ShopTradeFailureReason::ItemNotSellable;
        return preview;
    }

    if (slot_index < 0 || slot_index >= inventory->slotCount()) {
        preview.failure_reason = ShopTradeFailureReason::SlotMismatch;
        preview.unit_price = sell_rule->sell_price_;
        return preview;
    }

    const auto& stack = inventory->slot(slot_index);
    if (stack.empty() || stack.item_id_ != item_id) {
        preview.failure_reason = ShopTradeFailureReason::SlotMismatch;
        preview.unit_price = sell_rule->sell_price_;
        return preview;
    }

    preview.resolved_quantity = quantity;
    preview.unit_price = sell_rule->sell_price_;

    if (stack.count_ < preview.resolved_quantity) {
        preview.failure_reason = ShopTradeFailureReason::InsufficientItemCount;
        return preview;
    }
    if (!checkedMultiplyNonNegative(preview.unit_price, preview.resolved_quantity, preview.total_price) ||
        !checkedAddNonNegative(wallet->gold_, preview.total_price, preview.final_gold_after)) {
        preview.failure_reason = ShopTradeFailureReason::InvalidQuantity;
        return preview;
    }

    preview.failure_reason = ShopTradeFailureReason::None;
    return preview;
}

ShopSellResult ShopTransactionService::commitSell(const entt::entity player,
                                                  const entt::id_type item_id,
                                                  const int quantity,
                                                  const int slot_index) const {
    const ShopSellPreview preview = previewSell(player, item_id, quantity, slot_index);
    const auto* wallet = (player != entt::null && registry_.valid(player))
                             ? registry_.try_get<game::component::PlayerWalletComponent>(player)
                             : nullptr;
    const int current_gold = wallet ? wallet->gold_ : 0;
    if (!preview.canCommit()) {
        return makeSellResultFromFailure(preview, current_gold);
    }

    auto* mutable_wallet = registry_.try_get<game::component::PlayerWalletComponent>(player);
    if (mutable_wallet == nullptr) {
        ShopSellPreview failed_preview = preview;
        failed_preview.failure_reason = ShopTradeFailureReason::InvalidPlayer;
        return makeSellResultFromFailure(failed_preview, 0);
    }

    const auto mutation = inventory_domain_service_.removeItem(player, item_id, preview.resolved_quantity, slot_index);
    if (mutation.accepted != preview.resolved_quantity || mutation.rejected != 0) {
        spdlog::warn("ShopTransactionService: sell commit 结果异常 item_id={}, requested={}, accepted={}, rejected={}",
                     item_id,
                     preview.resolved_quantity,
                     mutation.accepted,
                     mutation.rejected);
        ShopSellPreview failed_preview = preview;
        failed_preview.failure_reason = ShopTradeFailureReason::InsufficientItemCount;
        return makeSellResultFromFailure(failed_preview, mutable_wallet->gold_);
    }

    mutable_wallet->gold_ += preview.total_price;

    ShopSellResult result{};
    fillTradeFields(
        result,
        preview.requested_quantity,
        preview.resolved_quantity,
        preview.unit_price,
        preview.total_price,
        mutable_wallet->gold_,
        ShopTradeFailureReason::None);
    return result;
}

} // namespace game::domain
