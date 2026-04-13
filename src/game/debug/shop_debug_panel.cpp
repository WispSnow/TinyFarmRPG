#include "game/debug/shop_debug_panel.h"

#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/data/shop_data.h"
#include "game/debug/shop_debug_panel_helpers.h"
#include "game/domain/shop_transaction_service.h"
#include "game/scene/shop_menu_scene.h"
#include "game/ui/shop_menu_support.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/utils/events.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <imgui.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] entt::entity findShopDebugPlayer(entt::registry& registry) {
    auto view = registry.view<
        game::component::PlayerTag,
        game::component::InventoryComponent,
        game::component::PlayerWalletComponent>();
    if (view.begin() == view.end()) {
        return entt::null;
    }
    return *view.begin();
}

[[nodiscard]] int countUsedSlots(const game::component::InventoryComponent& inventory) {
    int used_slots = 0;
    for (const auto& slot : inventory.slots_) {
        if (!slot.empty()) {
            ++used_slots;
        }
    }
    return used_slots;
}

[[nodiscard]] std::vector<const game::data::ShopData*> collectSortedShops(const game::data::ShopCatalog& shop_catalog) {
    auto shops = shop_catalog.listShops();
    std::sort(shops.begin(), shops.end(), [](const game::data::ShopData* lhs, const game::data::ShopData* rhs) {
        if (lhs == nullptr || rhs == nullptr) {
            return lhs != nullptr;
        }
        if (lhs->title_ != rhs->title_) {
            return lhs->title_ < rhs->title_;
        }
        return lhs->id_ < rhs->id_;
    });
    return shops;
}

void syncSelectedShopId(std::string& selected_shop_id, const std::vector<const game::data::ShopData*>& shops) {
    const auto found_it = std::find_if(shops.begin(), shops.end(), [&selected_shop_id](const auto* shop) {
        return shop != nullptr && shop->id_ == selected_shop_id;
    });
    if (found_it != shops.end()) {
        return;
    }

    if (!shops.empty() && shops.front() != nullptr) {
        selected_shop_id = shops.front()->id_;
        return;
    }

    selected_shop_id.clear();
}

[[nodiscard]] int clampSelectionIndex(const int current_index, const int row_count) {
    if (row_count <= 0) {
        return -1;
    }
    return std::clamp(current_index, 0, row_count - 1);
}

[[nodiscard]] std::string formatGoldValue(const int value) {
    return std::to_string(value) + " G";
}

[[nodiscard]] std::string buildTradeResultStatus(const game::debug::ShopDebugTradeMode mode,
                                                 const std::string_view item_name,
                                                 const int resolved_quantity,
                                                 const bool success,
                                                 const game::domain::ShopTradeFailureReason reason) {
    if (!success) {
        return std::string{game::debug::shopDebugTradeFailureLabel(mode, reason)};
    }

    std::string status = mode == game::debug::ShopDebugTradeMode::Buy ? "Bought " : "Sold ";
    status.append(item_name);
    status.append(" x");
    status.append(std::to_string(resolved_quantity));
    status.push_back('.');
    return status;
}

[[nodiscard]] const game::data::ItemData* findItemData(const game::data::ItemCatalog* item_catalog, const entt::id_type item_id) {
    return item_catalog != nullptr ? item_catalog->findItem(item_id) : nullptr;
}

[[nodiscard]] std::string resolveItemDescription(const game::data::ItemCatalog* item_catalog, const entt::id_type item_id) {
    if (const auto* item = findItemData(item_catalog, item_id)) {
        return item->description_;
    }
    return {};
}

} // namespace

namespace game::debug {

ShopDebugPanel::ShopDebugPanel(engine::core::Context& context,
                               entt::registry& registry,
                               const game::data::ShopCatalog* shop_catalog,
                               game::data::ItemCatalog* item_catalog,
                               game::domain::ShopTransactionService* shop_transaction_service)
    : context_(context),
      registry_(registry),
      shop_catalog_(shop_catalog),
      item_catalog_(item_catalog),
      shop_transaction_service_(shop_transaction_service) {
}

std::string_view ShopDebugPanel::name() const {
    return "Shops";
}

void ShopDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1100.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Shop Debug", &is_open)) {
        ImGui::End();
        return;
    }

    const entt::entity player = findShopDebugPlayer(registry_);
    if (player == entt::null) {
        ImGui::TextUnformatted("未找到带 InventoryComponent / PlayerWalletComponent 的玩家实体");
        ImGui::End();
        return;
    }
    if (shop_catalog_ == nullptr) {
        ImGui::TextUnformatted("ShopCatalog 不可用");
        ImGui::End();
        return;
    }
    if (item_catalog_ == nullptr) {
        ImGui::TextUnformatted("ItemCatalog 不可用");
        ImGui::End();
        return;
    }
    if (shop_transaction_service_ == nullptr) {
        ImGui::TextUnformatted("ShopTransactionService 不可用");
        ImGui::End();
        return;
    }

    auto* inventory = registry_.try_get<game::component::InventoryComponent>(player);
    auto* wallet = registry_.try_get<game::component::PlayerWalletComponent>(player);
    if (inventory == nullptr || wallet == nullptr) {
        ImGui::TextUnformatted("玩家缺少 InventoryComponent 或 PlayerWalletComponent");
        ImGui::End();
        return;
    }

    gold_seed_step_ = std::max(gold_seed_step_, 1);

    const auto shops = collectSortedShops(*shop_catalog_);
    syncSelectedShopId(selected_shop_id_, shops);
    const game::data::ShopData* selected_shop = shop_catalog_->findShop(selected_shop_id_);
    const bool scene_launch_blocked = context_.getGameState().isPaused();

    std::vector<ShopDebugBuyRow> buy_rows{};
    if (selected_shop != nullptr) {
        buy_rows.reserve(selected_shop->buy_entries_.size());
        for (const auto& buy_entry : selected_shop->buy_entries_) {
            buy_rows.push_back(buildShopDebugBuyRow(
                buy_entry,
                item_catalog_,
                game::ui::countOwnedItems(*inventory, buy_entry.item_id_hash_)));
        }
    }

    std::vector<ShopDebugSellRow> sell_rows{};
    sell_rows.reserve(static_cast<std::size_t>(inventory->slotCount()));
    for (int slot_index = 0; slot_index < inventory->slotCount(); ++slot_index) {
        const auto& stack = inventory->slot(slot_index);
        if (stack.empty()) {
            continue;
        }

        const auto* sell_rule = shop_catalog_->findSellRule(stack.item_id_);
        const auto row = buildShopDebugSellRow(slot_index, stack, sell_rule, item_catalog_);
        if (!show_unsellable_slots_ && !row.is_sellable) {
            continue;
        }
        sell_rows.push_back(row);
    }

    selected_buy_index_ = clampSelectionIndex(selected_buy_index_, static_cast<int>(buy_rows.size()));
    selected_sell_index_ = clampSelectionIndex(selected_sell_index_, static_cast<int>(sell_rows.size()));

    const ShopDebugBuyRow* selected_buy_row =
        (selected_buy_index_ >= 0 && selected_buy_index_ < static_cast<int>(buy_rows.size())) ? &buy_rows[static_cast<std::size_t>(selected_buy_index_)] : nullptr;
    const ShopDebugSellRow* selected_sell_row =
        (selected_sell_index_ >= 0 && selected_sell_index_ < static_cast<int>(sell_rows.size())) ? &sell_rows[static_cast<std::size_t>(selected_sell_index_)] : nullptr;

    if (selected_buy_row != nullptr) {
        const auto* selected_buy_item = findItemData(item_catalog_, selected_buy_row->item_id_hash);
        requested_buy_quantity_ = clampShopDebugQuantity(
            requested_buy_quantity_,
            selected_buy_item != nullptr ? game::ui::resolveBuyQuantityUiMax(*selected_buy_item) : 1);
    } else {
        requested_buy_quantity_ = 1;
    }

    if (selected_sell_row != nullptr) {
        requested_sell_quantity_ = clampShopDebugQuantity(
            requested_sell_quantity_,
            game::ui::resolveSellQuantityUiMax(inventory->slot(selected_sell_row->slot_index)));
    } else {
        requested_sell_quantity_ = 1;
    }

    ImGui::Text("Player: %u", static_cast<unsigned>(entt::to_integral(player)));
    ImGui::SameLine();
    ImGui::Text("Gold: %d", wallet->gold_);
    ImGui::SameLine();
    ImGui::Text("Inventory: %d / %d", countUsedSlots(*inventory), inventory->slotCount());
    ImGui::SameLine();
    ImGui::Text("Shop: %s", selected_shop != nullptr ? selected_shop->title_.c_str() : "<none>");
    if (selected_shop != nullptr) {
        ImGui::TextDisabled("%s", selected_shop->id_.c_str());
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Debug Wallet Seed");
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Gold Step", &gold_seed_step_);
    gold_seed_step_ = std::max(gold_seed_step_, 1);
    if (ImGui::Button("+Step")) {
        wallet->gold_ = std::max(0, wallet->gold_ + gold_seed_step_);
        status_ = "Adjusted gold by +" + std::to_string(gold_seed_step_) + ".";
    }
    ImGui::SameLine();
    if (ImGui::Button("Set Gold")) {
        wallet->gold_ = std::max(0, gold_seed_step_);
        status_ = "Set gold to " + std::to_string(wallet->gold_) + ".";
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(selected_shop == nullptr || scene_launch_blocked);
    if (ImGui::Button("Open Shop Scene")) {
        auto scene = std::make_unique<game::scene::ShopMenuScene>(
            "ShopMenu",
            context_,
            registry_,
            player,
            selected_shop->id_,
            shop_catalog_,
            item_catalog_,
            shop_transaction_service_);
        context_.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
        status_ = "Opened shop scene: " + selected_shop->title_ + ".";
        is_open = false;
    }
    ImGui::EndDisabled();
    if (selected_shop == nullptr) {
        ImGui::TextDisabled("Select a shop to open the in-game shop scene.");
    } else if (scene_launch_blocked) {
        ImGui::TextDisabled("Gameplay is paused; close the current overlay before opening another shop scene.");
    } else {
        ImGui::TextDisabled("Launches the real ShopMenuScene for this shop.");
    }

    ImGui::Separator();

    ImGui::BeginChild("ShopPicker", ImVec2(280.0f, 0.0f), true);
    ImGui::TextUnformatted("Shop Picker");
    ImGui::Separator();
    if (shops.empty()) {
        ImGui::TextWrapped("ShopCatalog 当前没有可用商店。");
    } else {
        for (const auto* shop : shops) {
            if (shop == nullptr) {
                continue;
            }
            const std::string label = shop->title_ + "##" + shop->id_;
            if (ImGui::Selectable(label.c_str(), selected_shop_id_ == shop->id_)) {
                selected_shop_id_ = shop->id_;
                selected_buy_index_ = 0;
                requested_buy_quantity_ = 1;
                requested_sell_quantity_ = 1;
                status_.clear();
            }
            ImGui::TextDisabled("%s", shop->id_.c_str());
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ShopWorkArea", ImVec2(0.0f, 0.0f), false);
    if (ImGui::BeginTabBar("ShopDebugTabs")) {
        if (ImGui::BeginTabItem("Buy", nullptr)) {
            const auto buy_preview = (selected_shop != nullptr && selected_buy_row != nullptr)
                                         ? shop_transaction_service_->previewBuy(
                                               player,
                                               selected_shop_id_,
                                               selected_buy_row->item_id_hash,
                                               requested_buy_quantity_)
                                         : game::domain::ShopBuyPreview{};

            ImGui::BeginChild("BuyRows", ImVec2(360.0f, 0.0f), true);
            if (selected_shop == nullptr) {
                ImGui::TextWrapped("未选择商店。");
            } else if (buy_rows.empty()) {
                ImGui::TextWrapped("当前商店没有 buy entries。");
            } else {
                for (int i = 0; i < static_cast<int>(buy_rows.size()); ++i) {
                    const auto& row = buy_rows[static_cast<std::size_t>(i)];
                    const std::string label = row.item_name + "##buy_" + row.item_id;
                    if (ImGui::Selectable(label.c_str(), selected_buy_index_ == i)) {
                        selected_buy_index_ = i;
                        requested_buy_quantity_ = 1;
                        status_.clear();
                    }
                    ImGui::TextDisabled("%s | %s | owned %d", row.item_id.c_str(), formatGoldValue(row.buy_price).c_str(), row.owned_count);
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("BuyDetail", ImVec2(0.0f, 0.0f), true);
            if (selected_buy_row == nullptr || selected_shop == nullptr) {
                ImGui::TextWrapped("选择一件商品以查看 buy preview。");
            } else {
                ImGui::Text("Item: %s", selected_buy_row->item_name.c_str());
                ImGui::TextDisabled("%s", selected_buy_row->item_id.c_str());
                const std::string description = resolveItemDescription(item_catalog_, selected_buy_row->item_id_hash);
                if (!description.empty()) {
                    ImGui::TextWrapped("%s", description.c_str());
                }
                ImGui::Separator();
                ImGui::Text("Owned: %d", selected_buy_row->owned_count);
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputInt("Buy Quantity", &requested_buy_quantity_)) {
                    const auto* selected_buy_item = findItemData(item_catalog_, selected_buy_row->item_id_hash);
                    requested_buy_quantity_ = clampShopDebugQuantity(
                        requested_buy_quantity_,
                        selected_buy_item != nullptr ? game::ui::resolveBuyQuantityUiMax(*selected_buy_item) : 1);
                    status_.clear();
                }
                ImGui::Text("Unit Price: %s", formatGoldValue(buy_preview.unit_price).c_str());
                ImGui::Text("Total Price: %s", formatGoldValue(buy_preview.total_price).c_str());
                ImGui::Text("Gold After: %s", formatGoldValue(buy_preview.final_gold_after).c_str());
                ImGui::Text("Preview: %s",
                            shopDebugTradeFailureLabel(ShopDebugTradeMode::Buy, buy_preview.failure_reason).data());

                ImGui::BeginDisabled(!buy_preview.canCommit());
                if (ImGui::Button("Commit Buy")) {
                    const auto result = shop_transaction_service_->commitBuy(
                        player,
                        selected_shop_id_,
                        selected_buy_row->item_id_hash,
                        requested_buy_quantity_);
                    requested_buy_quantity_ = 1;
                    status_ = buildTradeResultStatus(
                        ShopDebugTradeMode::Buy,
                        selected_buy_row->item_name,
                        result.resolved_quantity,
                        result.completed(),
                        result.failure_reason);
                }
                ImGui::EndDisabled();
            }
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Sell", nullptr)) {
            const auto sell_preview = (selected_sell_row != nullptr)
                                          ? shop_transaction_service_->previewSell(
                                                player,
                                                selected_sell_row->item_id_hash,
                                                requested_sell_quantity_,
                                                selected_sell_row->slot_index)
                                          : game::domain::ShopSellPreview{};

            if (ImGui::Checkbox("Show Unsellable Slots", &show_unsellable_slots_)) {
                selected_sell_index_ = 0;
                requested_sell_quantity_ = 1;
                status_.clear();
            }

            ImGui::BeginChild("SellRows", ImVec2(360.0f, 0.0f), true);
            if (sell_rows.empty()) {
                ImGui::TextWrapped(show_unsellable_slots_ ? "当前没有可见的非空槽位。" : "当前没有可卖物品。");
            } else {
                for (int i = 0; i < static_cast<int>(sell_rows.size()); ++i) {
                    const auto& row = sell_rows[static_cast<std::size_t>(i)];
                    const std::string label =
                        row.item_name + "##sell_slot_" + std::to_string(row.slot_index) + "_" + std::to_string(i);
                    ImGui::BeginDisabled(!row.is_sellable);
                    if (ImGui::Selectable(label.c_str(), selected_sell_index_ == i)) {
                        selected_sell_index_ = i;
                        requested_sell_quantity_ = 1;
                        status_.clear();
                    }
                    ImGui::EndDisabled();
                    ImGui::TextDisabled(
                        "slot %d | x%d | %s",
                        row.slot_index,
                        row.stack_count,
                        row.is_sellable ? formatGoldValue(row.sell_price).c_str() : "--");
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("SellDetail", ImVec2(0.0f, 0.0f), true);
            if (selected_sell_row == nullptr) {
                ImGui::TextWrapped("选择一个背包槽位以查看 sell preview。");
            } else {
                ImGui::Text("Item: %s", selected_sell_row->item_name.c_str());
                ImGui::TextDisabled("%s | slot %d", selected_sell_row->item_id.c_str(), selected_sell_row->slot_index);
                const std::string description = resolveItemDescription(item_catalog_, selected_sell_row->item_id_hash);
                if (!description.empty()) {
                    ImGui::TextWrapped("%s", description.c_str());
                }
                ImGui::Separator();
                ImGui::Text("In Slot: %d", selected_sell_row->stack_count);
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputInt("Sell Quantity", &requested_sell_quantity_)) {
                    requested_sell_quantity_ = clampShopDebugQuantity(
                        requested_sell_quantity_,
                        game::ui::resolveSellQuantityUiMax(inventory->slot(selected_sell_row->slot_index)));
                    status_.clear();
                }
                ImGui::Text("Unit Price: %s",
                            selected_sell_row->is_sellable ? formatGoldValue(sell_preview.unit_price).c_str() : "--");
                ImGui::Text("Total Price: %s",
                            selected_sell_row->is_sellable ? formatGoldValue(sell_preview.total_price).c_str() : "--");
                ImGui::Text("Gold After: %s", formatGoldValue(sell_preview.final_gold_after).c_str());
                ImGui::Text("Preview: %s",
                            shopDebugTradeFailureLabel(ShopDebugTradeMode::Sell, sell_preview.failure_reason).data());

                ImGui::BeginDisabled(!selected_sell_row->is_sellable || !sell_preview.canCommit());
                if (ImGui::Button("Commit Sell")) {
                    const int sold_slot_index = selected_sell_row->slot_index;
                    const auto result = shop_transaction_service_->commitSell(
                        player,
                        selected_sell_row->item_id_hash,
                        requested_sell_quantity_,
                        sold_slot_index);
                    requested_sell_quantity_ = 1;
                    status_ = buildTradeResultStatus(
                        ShopDebugTradeMode::Sell,
                        selected_sell_row->item_name,
                        result.resolved_quantity,
                        result.completed(),
                        result.failure_reason);
                }
                ImGui::EndDisabled();
            }
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (!status_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_.c_str());
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace game::debug
