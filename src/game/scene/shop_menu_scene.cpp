#include "game/scene/shop_menu_scene.h"

#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/data/shop_data.h"
#include "game/ui/slot_grid_support.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <utility>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/shop_menu.rml";
constexpr std::string_view MODEL_NAME = "shop_menu";

using ShopMenuMode = game::scene::ShopMenuScene::ShopMenuMode;
using ShopMenuFocusArea = game::ui::ShopMenuFocusArea;
using ShopMenuNavigationDecision = game::ui::ShopMenuNavigationDecision;
using ShopMenuNavigationInput = game::ui::ShopMenuNavigationInput;
using ShopMenuNavigationState = game::ui::ShopMenuNavigationState;
using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;
using namespace entt::literals;

void updateStringBinding(engine::ui::rmlui::RmlDocumentController& controller,
                         std::string_view variable_name,
                         Rml::String& current,
                         std::string_view next) {
    if (updateBoundString(current, next)) {
        controller.markDirty(variable_name);
    }
}

void updateBoolBinding(engine::ui::rmlui::RmlDocumentController& controller,
                       std::string_view variable_name,
                       bool& current,
                       const bool next) {
    if (updateBoundBool(current, next)) {
        controller.markDirty(variable_name);
    }
}

[[nodiscard]] int clampSelectionIndex(const int index, const int size) {
    if (size <= 0) {
        return 0;
    }
    return std::clamp(index, 0, size - 1);
}

[[nodiscard]] std::string formatGoldLabel(const int gold) {
    return "Gold: " + std::to_string(gold);
}

[[nodiscard]] std::string formatGoldValue(const int gold) {
    return std::to_string(gold) + " G";
}

[[nodiscard]] std::string formatQuantityText(const int quantity) {
    return "x" + std::to_string(std::max(0, quantity));
}

[[nodiscard]] std::string_view defaultEmptyText(const ShopMenuMode mode) {
    return mode == ShopMenuMode::Buy ? "This shop has no goods available." : "You have nothing to sell.";
}

[[nodiscard]] std::string formatFocusStatus(const ShopMenuMode mode,
                                            const ShopMenuFocusArea focus_area,
                                            const bool quantity_adjustable) {
    switch (focus_area) {
        case ShopMenuFocusArea::ModeToggle:
            return "Left / Right switches Buy and Sell. Down enters the list.";
        case ShopMenuFocusArea::EntryList:
            return mode == ShopMenuMode::Buy ? "Up / Down selects. Right opens quantity. Confirm goes to Buy."
                                             : "Up / Down selects. Right opens quantity. Confirm goes to Sell.";
        case ShopMenuFocusArea::Quantity:
            if (!quantity_adjustable) {
                return mode == ShopMenuMode::Buy ? "Quantity is fixed at x1. Down or Confirm goes to Buy."
                                                 : "Quantity is fixed at x1. Down or Confirm goes to Sell.";
            }
            return mode == ShopMenuMode::Buy ? "Left / Right changes quantity. Down or Confirm goes to Buy."
                                             : "Left / Right changes quantity. Down or Confirm goes to Sell.";
        case ShopMenuFocusArea::PrimaryAction:
            return mode == ShopMenuMode::Buy ? "Confirm to buy. Left returns to the list."
                                             : "Confirm to sell. Left returns to the list.";
    }

    return mode == ShopMenuMode::Buy ? "Confirm to buy." : "Confirm to sell.";
}

[[nodiscard]] std::string formatFailureText(const ShopMenuMode mode, const game::domain::ShopTradeFailureReason failure_reason) {
    using game::domain::ShopTradeFailureReason;

    switch (failure_reason) {
        case ShopTradeFailureReason::None:
            break;
        case ShopTradeFailureReason::InsufficientGold:
            return "Not enough gold.";
        case ShopTradeFailureReason::InventoryFull:
            return mode == ShopMenuMode::Buy ? "Inventory is full." : "Action unavailable.";
        case ShopTradeFailureReason::InvalidQuantity:
            return "Invalid quantity.";
        case ShopTradeFailureReason::InvalidPlayer:
            return "Action unavailable.";
        case ShopTradeFailureReason::InvalidShop:
            return mode == ShopMenuMode::Buy ? "This shop is unavailable." : "Action unavailable.";
        case ShopTradeFailureReason::InvalidItem:
            return "Action unavailable.";
        case ShopTradeFailureReason::ItemNotSoldHere:
            return "This item cannot be purchased here.";
        case ShopTradeFailureReason::ItemNotSellable:
            return "This item cannot be sold.";
        case ShopTradeFailureReason::SlotMismatch:
            return "This slot changed.";
        case ShopTradeFailureReason::InsufficientItemCount:
            return "Not enough items in this slot.";
    }

    return mode == ShopMenuMode::Buy ? "Purchase failed." : "Sale failed.";
}

[[nodiscard]] std::string formatSuccessText(const ShopMenuMode mode,
                                            std::string_view item_name,
                                            const int quantity) {
    std::string text = mode == ShopMenuMode::Buy ? "Purchased " : "Sold ";
    text.append(item_name);
    if (quantity > 1) {
        text.push_back(' ');
        text.push_back('x');
        text += std::to_string(quantity);
    }
    text.push_back('.');
    return text;
}

} // namespace

namespace game::scene {

ShopMenuScene::ShopMenuScene(std::string_view name,
                             engine::core::Context& context,
                             entt::registry& game_registry,
                             const entt::entity player,
                             std::string shop_id,
                             const game::data::ShopCatalog* shop_catalog,
                             game::data::ItemCatalog* item_catalog,
                             game::domain::ShopTransactionService* shop_transaction_service)
    : engine::scene::Scene(name, context),
      game_registry_(game_registry),
      player_(player),
      shop_id_(std::move(shop_id)),
      shop_catalog_(shop_catalog),
      item_catalog_(item_catalog),
      shop_transaction_service_(shop_transaction_service),
      previous_state_(context.getGameState().getCurrentState()) {
}

ShopMenuScene::~ShopMenuScene() {
    disconnectRuntimeListeners();
    shutdownUI();
    if (context_pushed_) {
        context_.getGameState().setState(previous_state_);
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
}

bool ShopMenuScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    const auto rollback_init = [this]() {
        disconnectRuntimeListeners();
        shutdownUI();
        context_.getGameState().setState(previous_state_);
        if (context_pushed_) {
            context_.getInputManager().popContext();
            context_pushed_ = false;
        }
    };

    if (!shop_catalog_ || !item_catalog_ || !shop_transaction_service_) {
        spdlog::error("ShopMenuScene: gameplay 依赖未完整注入。");
        rollback_init();
        return false;
    }
    if (player_ == entt::null || !game_registry_.valid(player_)) {
        spdlog::error("ShopMenuScene: player 实体无效。");
        rollback_init();
        return false;
    }

    shop_data_ = shop_catalog_->findShop(shop_id_);
    if (!shop_data_) {
        spdlog::error("ShopMenuScene: shop_id='{}' 未在 ShopCatalog 中找到。", shop_id_);
        rollback_init();
        return false;
    }

    shop_title_ = shop_data_->title_.empty() ? Rml::String{"Shop"} : Rml::String{shop_data_->title_};
    shop_greeting_ = shop_data_->greeting_.empty() ? Rml::String{"Welcome."} : Rml::String{shop_data_->greeting_};

    if (!initUI()) {
        rollback_init();
        return false;
    }

    refreshAll();

    if (!Scene::init()) {
        rollback_init();
        return false;
    }

    connectRuntimeListeners();
    return true;
}

void ShopMenuScene::clean() {
    disconnectRuntimeListeners();
    shutdownUI();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool ShopMenuScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("ShopMenuScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("ShopMenuScene: 创建 data model 失败。");
        return false;
    }

    if (!data_types_registered_) {
        if (!game::ui::registerShopBuyEntryViewModelType(constructor) ||
            !constructor.RegisterArray<decltype(buy_entries_)>() ||
            !game::ui::registerShopSellEntryViewModelType(constructor) ||
            !constructor.RegisterArray<decltype(sell_entries_)>()) {
            spdlog::error("ShopMenuScene: 注册 ShopMenu data types 失败。");
            document_controller_.unload();
            return false;
        }
        data_types_registered_ = true;
    }

    if (!constructor.Bind("shop_title", &shop_title_) ||
        !constructor.Bind("shop_greeting", &shop_greeting_) ||
        !constructor.Bind("gold_label", &gold_label_) ||
        !constructor.Bind("status_text", &status_text_) ||
        !constructor.Bind("list_title_text", &list_title_text_) ||
        !constructor.Bind("empty_text", &empty_text_) ||
        !constructor.Bind("detail_name", &detail_name_) ||
        !constructor.Bind("detail_description", &detail_description_) ||
        !constructor.Bind("detail_price_text", &detail_price_text_) ||
        !constructor.Bind("detail_total_text", &detail_total_text_) ||
        !constructor.Bind("detail_after_gold_text", &detail_after_gold_text_) ||
        !constructor.Bind("detail_quantity_text", &detail_quantity_text_) ||
        !constructor.Bind("detail_owned_label", &detail_owned_label_) ||
        !constructor.Bind("detail_owned_text", &detail_owned_text_) ||
        !constructor.Bind("is_buy_mode", &is_buy_mode_) ||
        !constructor.Bind("is_sell_mode", &is_sell_mode_) ||
        !constructor.Bind("is_mode_toggle_focused", &is_mode_toggle_focused_) ||
        !constructor.Bind("is_entry_list_focused", &is_entry_list_focused_) ||
        !constructor.Bind("is_quantity_focused", &is_quantity_focused_) ||
        !constructor.Bind("is_primary_action_focused", &is_primary_action_focused_) ||
        !constructor.Bind("has_buy_entries", &has_buy_entries_) ||
        !constructor.Bind("has_sell_entries", &has_sell_entries_) ||
        !constructor.Bind("buy_enabled", &buy_enabled_) ||
        !constructor.Bind("sell_enabled", &sell_enabled_) ||
        !constructor.Bind("primary_action_enabled", &primary_action_enabled_) ||
        !constructor.Bind("primary_action_text", &primary_action_text_) ||
        !constructor.Bind("quantity_decrease_enabled", &quantity_decrease_enabled_) ||
        !constructor.Bind("quantity_increase_enabled", &quantity_increase_enabled_) ||
        !constructor.Bind("buy_entries", &buy_entries_) ||
        !constructor.Bind("sell_entries", &sell_entries_)) {
        spdlog::error("ShopMenuScene: 绑定 data model 变量失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.bindEvent(
            constructor,
            "buy_entry_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                selectBuyEntry(game::ui::getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "sell_entry_select",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                selectSellEntry(game::ui::getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "adjust_quantity",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                adjustQuantity(game::ui::getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindSimpleEvent(constructor, "switch_mode_buy", [this] { switchMode(ShopMenuMode::Buy); }) ||
        !document_controller_.bindSimpleEvent(constructor, "switch_mode_sell", [this] { switchMode(ShopMenuMode::Sell); }) ||
        !document_controller_.bindSimpleEvent(
            constructor,
            "confirm_trade",
            [this] { current_mode_ == ShopMenuMode::Buy ? confirmBuy() : confirmSell(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "close", [this] { onClose(); })) {
        spdlog::error("ShopMenuScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("ShopMenuScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    document_controller_.markAllDirty();
    return true;
}

void ShopMenuScene::shutdownUI() {
    document_controller_.unload();
}

void ShopMenuScene::connectRuntimeListeners() {
    if (input_listeners_connected_) {
        return;
    }

    auto& input_manager = context_.getInputManager();
    input_manager.onAction("menu_up"_hs).connect<&ShopMenuScene::onMenuUpPressed>(this);
    input_manager.onAction("menu_down"_hs).connect<&ShopMenuScene::onMenuDownPressed>(this);
    input_manager.onAction("menu_left"_hs).connect<&ShopMenuScene::onMenuLeftPressed>(this);
    input_manager.onAction("menu_right"_hs).connect<&ShopMenuScene::onMenuRightPressed>(this);
    input_manager.onAction("menu_confirm"_hs).connect<&ShopMenuScene::onMenuConfirmPressed>(this);
    input_manager.onAction("menu_cancel"_hs).connect<&ShopMenuScene::onMenuCancelPressed>(this);
    input_listeners_connected_ = true;
}

void ShopMenuScene::disconnectRuntimeListeners() {
    if (!input_listeners_connected_) {
        return;
    }

    auto& input_manager = context_.getInputManager();
    input_manager.onAction("menu_up"_hs).disconnect<&ShopMenuScene::onMenuUpPressed>(this);
    input_manager.onAction("menu_down"_hs).disconnect<&ShopMenuScene::onMenuDownPressed>(this);
    input_manager.onAction("menu_left"_hs).disconnect<&ShopMenuScene::onMenuLeftPressed>(this);
    input_manager.onAction("menu_right"_hs).disconnect<&ShopMenuScene::onMenuRightPressed>(this);
    input_manager.onAction("menu_confirm"_hs).disconnect<&ShopMenuScene::onMenuConfirmPressed>(this);
    input_manager.onAction("menu_cancel"_hs).disconnect<&ShopMenuScene::onMenuCancelPressed>(this);
    input_listeners_connected_ = false;
}

void ShopMenuScene::syncGoldLabel() {
    const auto* wallet = game_registry_.try_get<game::component::PlayerWalletComponent>(player_);
    updateStringBinding(document_controller_, "gold_label", gold_label_, formatGoldLabel(wallet ? wallet->gold_ : 0));
}

void ShopMenuScene::syncModeBindings() {
    const bool is_buy_mode = current_mode_ == ShopMenuMode::Buy;
    updateBoolBinding(document_controller_, "is_buy_mode", is_buy_mode_, is_buy_mode);
    updateBoolBinding(document_controller_, "is_sell_mode", is_sell_mode_, !is_buy_mode);
    updateStringBinding(document_controller_, "list_title_text", list_title_text_, is_buy_mode ? "Buy" : "Sell");
    updateStringBinding(
        document_controller_,
        "empty_text",
        empty_text_,
        is_buy_mode ? defaultEmptyText(ShopMenuMode::Buy) : defaultEmptyText(ShopMenuMode::Sell));
    updateStringBinding(
        document_controller_,
        "detail_owned_label",
        detail_owned_label_,
        is_buy_mode ? "Owned" : "In Slot");
    updateStringBinding(document_controller_, "primary_action_text", primary_action_text_, is_buy_mode ? "Buy" : "Sell");
    // This mirrors the active mode's preview validity instead of introducing a third independent action state.
    updateBoolBinding(
        document_controller_,
        "primary_action_enabled",
        primary_action_enabled_,
        is_buy_mode ? buy_enabled_ : sell_enabled_);
}

void ShopMenuScene::syncFocusBindings() {
    updateBoolBinding(
        document_controller_,
        "is_mode_toggle_focused",
        is_mode_toggle_focused_,
        current_focus_area_ == ShopMenuFocusArea::ModeToggle);
    updateBoolBinding(
        document_controller_,
        "is_entry_list_focused",
        is_entry_list_focused_,
        current_focus_area_ == ShopMenuFocusArea::EntryList);
    updateBoolBinding(
        document_controller_,
        "is_quantity_focused",
        is_quantity_focused_,
        current_focus_area_ == ShopMenuFocusArea::Quantity);
    updateBoolBinding(
        document_controller_,
        "is_primary_action_focused",
        is_primary_action_focused_,
        current_focus_area_ == ShopMenuFocusArea::PrimaryAction);
}

void ShopMenuScene::normalizeFocusArea() {
    if (!hasCurrentEntries()) {
        current_focus_area_ = game::ui::resolvePreferredShopMenuFocus(false);
    }
}

void ShopMenuScene::markTradeListsDirty() {
    buy_entries_dirty_ = true;
    sell_entries_dirty_ = true;
}

void ShopMenuScene::rebuildBuyEntries() {
    buy_entry_refs_.clear();
    buy_entries_.clear();

    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!shop_data_ || !item_catalog_) {
        updateBoolBinding(document_controller_, "has_buy_entries", has_buy_entries_, false);
        document_controller_.markDirty("buy_entries");
        buy_entries_dirty_ = false;
        return;
    }

    buy_entry_refs_.reserve(shop_data_->buy_entries_.size());
    buy_entries_.reserve(shop_data_->buy_entries_.size());

    for (const auto& buy_entry : shop_data_->buy_entries_) {
        const auto* item = item_catalog_->findItem(buy_entry.item_id_hash_);
        if (!item) {
            spdlog::error("ShopMenuScene: shop_id='{}' 的 buy entry item_id='{}' 在 ItemCatalog 中缺失，已跳过。",
                          shop_id_,
                          buy_entry.item_id_);
            continue;
        }

        const int owned_count = inventory ? game::ui::countOwnedItems(*inventory, buy_entry.item_id_hash_) : 0;
        game::ui::ShopBuyEntryViewModel view_model{};
        game::ui::populateShopBuyEntryViewModel(
            view_model,
            static_cast<int>(buy_entries_.size()),
            buy_entry,
            item_catalog_,
            owned_count,
            false,
            false);
        buy_entry_refs_.push_back(&buy_entry);
        buy_entries_.push_back(std::move(view_model));
    }

    updateBoolBinding(document_controller_, "has_buy_entries", has_buy_entries_, !buy_entries_.empty());
    selected_buy_index_ = clampSelectionIndex(selected_buy_index_, static_cast<int>(buy_entries_.size()));
    for (auto& entry : buy_entries_) {
        entry.is_selected = has_buy_entries_ && entry.index == selected_buy_index_;
    }

    document_controller_.markDirty("buy_entries");
    buy_entries_dirty_ = false;
}

void ShopMenuScene::rebuildSellEntries() {
    sell_entries_.clear();

    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory || !item_catalog_ || !shop_catalog_) {
        updateBoolBinding(document_controller_, "has_sell_entries", has_sell_entries_, false);
        document_controller_.markDirty("sell_entries");
        sell_entries_dirty_ = false;
        return;
    }

    sell_entries_.reserve(static_cast<std::size_t>(inventory->slotCount()));
    for (int slot_index = 0; slot_index < inventory->slotCount(); ++slot_index) {
        const auto& stack = inventory->slot(slot_index);
        if (stack.empty()) {
            continue;
        }

        game::ui::ShopSellEntryViewModel view_model{};
        game::ui::populateShopSellEntryViewModel(
            view_model,
            static_cast<int>(sell_entries_.size()),
            slot_index,
            stack,
            shop_catalog_->findSellRule(stack.item_id_),
            item_catalog_,
            false);
        sell_entries_.push_back(std::move(view_model));
    }

    updateBoolBinding(document_controller_, "has_sell_entries", has_sell_entries_, !sell_entries_.empty());
    selected_sell_index_ = clampSelectionIndex(selected_sell_index_, static_cast<int>(sell_entries_.size()));
    for (auto& entry : sell_entries_) {
        entry.is_selected = has_sell_entries_ && entry.index == selected_sell_index_;
    }

    document_controller_.markDirty("sell_entries");
    sell_entries_dirty_ = false;
}

void ShopMenuScene::refreshSelectedBuyEntry() {
    const auto* buy_entry = currentBuyEntry();
    const auto* item = currentBuyItemData();
    if (!buy_entry || !item) {
        requested_buy_quantity_ = 1;
        updateStringBinding(document_controller_, "detail_name", detail_name_, "No goods available");
        updateStringBinding(document_controller_, "detail_description", detail_description_, "This shop has no purchasable goods.");
        updateStringBinding(document_controller_, "detail_quantity_text", detail_quantity_text_, "x0");
        updateStringBinding(document_controller_, "detail_owned_text", detail_owned_text_, "0");
        updateBoolBinding(document_controller_, "quantity_decrease_enabled", quantity_decrease_enabled_, false);
        updateBoolBinding(document_controller_, "quantity_increase_enabled", quantity_increase_enabled_, false);
        return;
    }

    requested_buy_quantity_ = std::clamp(requested_buy_quantity_, 1, currentQuantityUiMax());

    updateStringBinding(
        document_controller_,
        "detail_name",
        detail_name_,
        item->display_name_.empty() ? buy_entry->item_id_ : item->display_name_);
    updateStringBinding(document_controller_, "detail_description", detail_description_, item->description_);
    updateStringBinding(
        document_controller_,
        "detail_quantity_text",
        detail_quantity_text_,
        formatQuantityText(requested_buy_quantity_));
    updateStringBinding(
        document_controller_,
        "detail_owned_text",
        detail_owned_text_,
        std::to_string(currentBuyOwnedCount()));

    const bool quantity_adjustable = currentQuantityUiMax() > 1;
    updateBoolBinding(
        document_controller_,
        "quantity_decrease_enabled",
        quantity_decrease_enabled_,
        quantity_adjustable && requested_buy_quantity_ > 1);
    updateBoolBinding(
        document_controller_,
        "quantity_increase_enabled",
        quantity_increase_enabled_,
        quantity_adjustable && requested_buy_quantity_ < currentQuantityUiMax());
}

void ShopMenuScene::refreshSelectedSellEntry() {
    const auto* sell_entry = currentSellEntry();
    const auto* item = currentSellItemData();
    if (!sell_entry || !item) {
        requested_sell_quantity_ = 1;
        updateStringBinding(document_controller_, "detail_name", detail_name_, "No items available");
        updateStringBinding(document_controller_, "detail_description", detail_description_, "You have nothing to sell.");
        updateStringBinding(document_controller_, "detail_quantity_text", detail_quantity_text_, "x0");
        updateStringBinding(document_controller_, "detail_owned_text", detail_owned_text_, "0");
        updateBoolBinding(document_controller_, "quantity_decrease_enabled", quantity_decrease_enabled_, false);
        updateBoolBinding(document_controller_, "quantity_increase_enabled", quantity_increase_enabled_, false);
        return;
    }

    requested_sell_quantity_ = std::clamp(requested_sell_quantity_, 1, currentQuantityUiMax());

    updateStringBinding(
        document_controller_,
        "detail_name",
        detail_name_,
        item->display_name_.empty() ? item->id_str_ : item->display_name_);
    updateStringBinding(document_controller_, "detail_description", detail_description_, item->description_);
    updateStringBinding(
        document_controller_,
        "detail_quantity_text",
        detail_quantity_text_,
        formatQuantityText(requested_sell_quantity_));
    updateStringBinding(
        document_controller_,
        "detail_owned_text",
        detail_owned_text_,
        std::to_string(currentSellSlotCount()));

    const bool quantity_adjustable = currentQuantityUiMax() > 1;
    updateBoolBinding(
        document_controller_,
        "quantity_decrease_enabled",
        quantity_decrease_enabled_,
        quantity_adjustable && requested_sell_quantity_ > 1);
    updateBoolBinding(
        document_controller_,
        "quantity_increase_enabled",
        quantity_increase_enabled_,
        quantity_adjustable && requested_sell_quantity_ < currentQuantityUiMax());
}

void ShopMenuScene::refreshBuyPreview() {
    const auto* buy_entry = currentBuyEntry();
    if (!buy_entry || !shop_transaction_service_) {
        active_buy_preview_ = {};
        updateStringBinding(document_controller_, "detail_price_text", detail_price_text_, "-");
        updateStringBinding(document_controller_, "detail_total_text", detail_total_text_, "-");
        updateStringBinding(document_controller_, "detail_after_gold_text", detail_after_gold_text_, "-");
        updateBoolBinding(document_controller_, "buy_enabled", buy_enabled_, false);
        return;
    }

    active_buy_preview_ = shop_transaction_service_->previewBuy(
        player_,
        shop_id_,
        buy_entry->item_id_hash_,
        requested_buy_quantity_);

    updateStringBinding(
        document_controller_,
        "detail_price_text",
        detail_price_text_,
        formatGoldValue(active_buy_preview_.unit_price));
    updateStringBinding(
        document_controller_,
        "detail_total_text",
        detail_total_text_,
        formatGoldValue(active_buy_preview_.total_price));
    updateStringBinding(
        document_controller_,
        "detail_after_gold_text",
        detail_after_gold_text_,
        formatGoldValue(active_buy_preview_.final_gold_after));
    updateBoolBinding(document_controller_, "buy_enabled", buy_enabled_, active_buy_preview_.canCommit());
}

void ShopMenuScene::refreshSellPreview() {
    const auto* sell_entry = currentSellEntry();
    const entt::id_type item_id = currentSellItemId();
    if (!sell_entry || item_id == entt::null || !shop_transaction_service_) {
        active_sell_preview_ = {};
        updateStringBinding(document_controller_, "detail_price_text", detail_price_text_, "-");
        updateStringBinding(document_controller_, "detail_total_text", detail_total_text_, "-");
        updateStringBinding(document_controller_, "detail_after_gold_text", detail_after_gold_text_, "-");
        updateBoolBinding(document_controller_, "sell_enabled", sell_enabled_, false);
        return;
    }

    active_sell_preview_ = shop_transaction_service_->previewSell(
        player_,
        item_id,
        requested_sell_quantity_,
        sell_entry->slot_index);

    if (sell_entry->is_disabled) {
        updateStringBinding(document_controller_, "detail_price_text", detail_price_text_, sell_entry->price_text);
        updateStringBinding(document_controller_, "detail_total_text", detail_total_text_, "-");
        updateStringBinding(
            document_controller_,
            "detail_after_gold_text",
            detail_after_gold_text_,
            formatGoldValue(active_sell_preview_.final_gold_after));
        updateBoolBinding(document_controller_, "sell_enabled", sell_enabled_, false);
        return;
    }

    updateStringBinding(
        document_controller_,
        "detail_price_text",
        detail_price_text_,
        formatGoldValue(active_sell_preview_.unit_price));
    updateStringBinding(
        document_controller_,
        "detail_total_text",
        detail_total_text_,
        formatGoldValue(active_sell_preview_.total_price));
    updateStringBinding(
        document_controller_,
        "detail_after_gold_text",
        detail_after_gold_text_,
        formatGoldValue(active_sell_preview_.final_gold_after));
    updateBoolBinding(document_controller_, "sell_enabled", sell_enabled_, active_sell_preview_.canCommit());
}

void ShopMenuScene::refreshStatusText() {
    if (status_override_.has_value()) {
        updateStringBinding(document_controller_, "status_text", status_text_, *status_override_);
        return;
    }

    if (!hasCurrentEntries()) {
        updateStringBinding(document_controller_, "status_text", status_text_, defaultEmptyText(current_mode_));
        return;
    }

    const auto failure_reason =
        current_mode_ == ShopMenuMode::Buy ? active_buy_preview_.failure_reason : active_sell_preview_.failure_reason;
    if (failure_reason == game::domain::ShopTradeFailureReason::None) {
        updateStringBinding(
            document_controller_,
            "status_text",
            status_text_,
            formatFocusStatus(current_mode_, current_focus_area_, isCurrentQuantityAdjustable()));
        return;
    }

    updateStringBinding(document_controller_, "status_text", status_text_, formatFailureText(current_mode_, failure_reason));
}

void ShopMenuScene::refreshAll() {
    syncGoldLabel();
    if (buy_entries_dirty_) {
        rebuildBuyEntries();
    }
    if (sell_entries_dirty_) {
        rebuildSellEntries();
    }

    normalizeFocusArea();

    if (current_mode_ == ShopMenuMode::Buy) {
        refreshSelectedBuyEntry();
        refreshBuyPreview();
    } else {
        refreshSelectedSellEntry();
        refreshSellPreview();
    }

    syncModeBindings();
    syncFocusBindings();
    refreshStatusText();
}

void ShopMenuScene::clearStatusOverride() {
    status_override_.reset();
}

const game::data::ShopBuyEntryData* ShopMenuScene::currentBuyEntry() const {
    if (!has_buy_entries_ || selected_buy_index_ < 0 ||
        selected_buy_index_ >= static_cast<int>(buy_entry_refs_.size())) {
        return nullptr;
    }

    return buy_entry_refs_[selected_buy_index_];
}

const game::data::ItemData* ShopMenuScene::currentBuyItemData() const {
    const auto* buy_entry = currentBuyEntry();
    if (!buy_entry || !item_catalog_) {
        return nullptr;
    }

    return item_catalog_->findItem(buy_entry->item_id_hash_);
}

const game::ui::ShopSellEntryViewModel* ShopMenuScene::currentSellEntry() const {
    if (!has_sell_entries_ || selected_sell_index_ < 0 || selected_sell_index_ >= static_cast<int>(sell_entries_.size())) {
        return nullptr;
    }

    return &sell_entries_[selected_sell_index_];
}

const game::data::ItemData* ShopMenuScene::currentSellItemData() const {
    const entt::id_type item_id = currentSellItemId();
    if (item_id == entt::null || !item_catalog_) {
        return nullptr;
    }

    return item_catalog_->findItem(item_id);
}

entt::id_type ShopMenuScene::currentSellItemId() const {
    const auto* sell_entry = currentSellEntry();
    if (sell_entry == nullptr) {
        return entt::null;
    }

    return sell_entry->item_id_hash;
}

int ShopMenuScene::currentSellSlotIndex() const {
    const auto* sell_entry = currentSellEntry();
    return sell_entry ? sell_entry->slot_index : -1;
}

int ShopMenuScene::currentBuyOwnedCount() const {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    const auto* buy_entry = currentBuyEntry();
    if (!inventory || !buy_entry) {
        return 0;
    }

    return game::ui::countOwnedItems(*inventory, buy_entry->item_id_hash_);
}

int ShopMenuScene::currentSellSlotCount() const {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    const int slot_index = currentSellSlotIndex();
    if (!inventory || slot_index < 0 || slot_index >= inventory->slotCount()) {
        return 0;
    }

    return std::max(0, inventory->slot(slot_index).count_);
}

bool ShopMenuScene::hasCurrentEntries() const {
    return current_mode_ == ShopMenuMode::Buy ? has_buy_entries_ : has_sell_entries_;
}

int ShopMenuScene::currentQuantityUiMax() const {
    if (current_mode_ == ShopMenuMode::Sell) {
        const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
        const int slot_index = currentSellSlotIndex();
        if (!inventory || slot_index < 0 || slot_index >= inventory->slotCount()) {
            return 1;
        }

        return game::ui::resolveSellQuantityUiMax(inventory->slot(slot_index));
    }

    if (const auto* item = currentBuyItemData()) {
        return game::ui::resolveBuyQuantityUiMax(*item);
    }
    return 1;
}

bool ShopMenuScene::isCurrentQuantityAdjustable() const {
    return hasCurrentEntries() && currentQuantityUiMax() > 1;
}

ShopMenuNavigationState ShopMenuScene::makeNavigationState() const {
    return ShopMenuNavigationState{
        .is_buy_mode = current_mode_ == ShopMenuMode::Buy,
        .focus_area = current_focus_area_,
        .has_buy_entries = has_buy_entries_,
        .has_sell_entries = has_sell_entries_,
        .quantity_adjustable = isCurrentQuantityAdjustable()};
}

void ShopMenuScene::applyFocusArea(const ShopMenuFocusArea next_focus_area) {
    if (current_focus_area_ == next_focus_area) {
        return;
    }

    current_focus_area_ = next_focus_area;
    syncFocusBindings();
    refreshStatusText();
}

void ShopMenuScene::applyNavigationDecision(const ShopMenuNavigationDecision& decision) {
    const bool mode_changed = decision.switch_mode &&
                              ((decision.next_is_buy_mode && current_mode_ != ShopMenuMode::Buy) ||
                               (!decision.next_is_buy_mode && current_mode_ != ShopMenuMode::Sell));
    const bool selection_changed = decision.entry_delta != 0 && hasCurrentEntries();
    const bool quantity_changed = decision.quantity_delta != 0 && hasCurrentEntries();
    if (mode_changed || decision.next_focus_area != current_focus_area_ || selection_changed || quantity_changed) {
        clearStatusOverride();
    }

    if (mode_changed) {
        switchMode(decision.next_is_buy_mode ? ShopMenuMode::Buy : ShopMenuMode::Sell);
    }

    if (selection_changed) {
        if (current_mode_ == ShopMenuMode::Buy) {
            const int entry_count = static_cast<int>(buy_entries_.size());
            selectBuyEntry((selected_buy_index_ + entry_count + decision.entry_delta) % entry_count);
        } else {
            const int entry_count = static_cast<int>(sell_entries_.size());
            selectSellEntry((selected_sell_index_ + entry_count + decision.entry_delta) % entry_count);
        }
    }

    if (quantity_changed) {
        adjustQuantity(decision.quantity_delta);
    }

    if (decision.next_focus_area != current_focus_area_) {
        applyFocusArea(decision.next_focus_area);
    }

    if (decision.confirm_trade) {
        current_mode_ == ShopMenuMode::Buy ? confirmBuy() : confirmSell();
    }
}

void ShopMenuScene::selectBuyEntry(const int index) {
    if (!has_buy_entries_ || index < 0 || index >= static_cast<int>(buy_entries_.size()) || index == selected_buy_index_) {
        return;
    }

    if (selected_buy_index_ >= 0 && selected_buy_index_ < static_cast<int>(buy_entries_.size())) {
        buy_entries_[selected_buy_index_].is_selected = false;
    }
    selected_buy_index_ = index;
    buy_entries_[selected_buy_index_].is_selected = true;
    requested_buy_quantity_ = 1;
    clearStatusOverride();
    document_controller_.markDirty("buy_entries");
    refreshSelectedBuyEntry();
    refreshBuyPreview();
    syncModeBindings();
    refreshStatusText();
}

void ShopMenuScene::selectSellEntry(const int index) {
    if (!has_sell_entries_ || index < 0 || index >= static_cast<int>(sell_entries_.size()) || index == selected_sell_index_) {
        return;
    }

    if (selected_sell_index_ >= 0 && selected_sell_index_ < static_cast<int>(sell_entries_.size())) {
        sell_entries_[selected_sell_index_].is_selected = false;
    }
    selected_sell_index_ = index;
    sell_entries_[selected_sell_index_].is_selected = true;
    requested_sell_quantity_ = 1;
    clearStatusOverride();
    document_controller_.markDirty("sell_entries");
    refreshSelectedSellEntry();
    refreshSellPreview();
    syncModeBindings();
    refreshStatusText();
}

void ShopMenuScene::adjustQuantity(const int delta) {
    if (delta == 0 || !hasCurrentEntries()) {
        return;
    }

    const int max_quantity = currentQuantityUiMax();
    int& requested_quantity =
        current_mode_ == ShopMenuMode::Buy ? requested_buy_quantity_ : requested_sell_quantity_;
    const int next_quantity = std::clamp(requested_quantity + delta, 1, max_quantity);
    if (next_quantity == requested_quantity) {
        return;
    }

    requested_quantity = next_quantity;
    clearStatusOverride();
    if (current_mode_ == ShopMenuMode::Buy) {
        refreshSelectedBuyEntry();
        refreshBuyPreview();
    } else {
        refreshSelectedSellEntry();
        refreshSellPreview();
    }
    syncModeBindings();
    refreshStatusText();
}

void ShopMenuScene::switchMode(const ShopMenuMode next_mode) {
    if (current_mode_ == next_mode) {
        return;
    }

    current_mode_ = next_mode;
    if (current_focus_area_ != ShopMenuFocusArea::ModeToggle) {
        current_focus_area_ = game::ui::resolvePreferredShopMenuFocus(hasCurrentEntries());
    }
    clearStatusOverride();
    refreshAll();
}

void ShopMenuScene::confirmBuy() {
    const auto* buy_entry = currentBuyEntry();
    const auto* item = currentBuyItemData();
    if (!buy_entry || !item || !shop_transaction_service_) {
        status_override_ = "No item selected.";
        refreshStatusText();
        return;
    }

    if (!active_buy_preview_.canCommit()) {
        status_override_ = formatFailureText(ShopMenuMode::Buy, active_buy_preview_.failure_reason);
        refreshStatusText();
        return;
    }

    const std::string_view item_name =
        item->display_name_.empty() ? std::string_view{buy_entry->item_id_} : std::string_view{item->display_name_};
    const auto result = shop_transaction_service_->commitBuy(
        player_,
        shop_id_,
        buy_entry->item_id_hash_,
        requested_buy_quantity_);
    if (result.completed()) {
        requested_buy_quantity_ = 1;
        status_override_ = formatSuccessText(ShopMenuMode::Buy, item_name, result.resolved_quantity);
    } else {
        status_override_ = formatFailureText(ShopMenuMode::Buy, result.failure_reason);
    }

    markTradeListsDirty();
    refreshAll();
}

void ShopMenuScene::confirmSell() {
    const auto* item = currentSellItemData();
    const entt::id_type item_id = currentSellItemId();
    const int slot_index = currentSellSlotIndex();
    if (!item || item_id == entt::null || slot_index < 0 || !shop_transaction_service_) {
        status_override_ = "No item selected.";
        refreshStatusText();
        return;
    }

    if (!active_sell_preview_.canCommit()) {
        status_override_ = formatFailureText(ShopMenuMode::Sell, active_sell_preview_.failure_reason);
        refreshStatusText();
        return;
    }

    const std::string_view item_name =
        item->display_name_.empty() ? std::string_view{item->id_str_} : std::string_view{item->display_name_};
    const auto result = shop_transaction_service_->commitSell(
        player_,
        item_id,
        requested_sell_quantity_,
        slot_index);
    if (result.completed()) {
        requested_sell_quantity_ = 1;
        status_override_ = formatSuccessText(ShopMenuMode::Sell, item_name, result.resolved_quantity);
    } else {
        status_override_ = formatFailureText(ShopMenuMode::Sell, result.failure_reason);
    }

    markTradeListsDirty();
    refreshAll();
}

bool ShopMenuScene::onMenuUpPressed() {
    applyNavigationDecision(game::ui::resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Up));
    return true;
}

bool ShopMenuScene::onMenuDownPressed() {
    applyNavigationDecision(game::ui::resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Down));
    return true;
}

bool ShopMenuScene::onMenuLeftPressed() {
    applyNavigationDecision(game::ui::resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Left));
    return true;
}

bool ShopMenuScene::onMenuRightPressed() {
    applyNavigationDecision(game::ui::resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Right));
    return true;
}

bool ShopMenuScene::onMenuConfirmPressed() {
    applyNavigationDecision(game::ui::resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Confirm));
    return true;
}

bool ShopMenuScene::onMenuCancelPressed() {
    requestPopScene();
    return true;
}

void ShopMenuScene::onClose() {
    requestPopScene();
}

} // namespace game::scene
