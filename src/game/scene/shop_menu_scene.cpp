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
constexpr std::string_view DEFAULT_READY_STATUS = "Confirm to buy. Left / Right changes quantity.";

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

[[nodiscard]] std::string formatFailureText(const game::domain::ShopTradeFailureReason failure_reason) {
    using game::domain::ShopTradeFailureReason;

    switch (failure_reason) {
        case ShopTradeFailureReason::None:
            return std::string{DEFAULT_READY_STATUS};
        case ShopTradeFailureReason::InsufficientGold:
            return "Not enough gold.";
        case ShopTradeFailureReason::InventoryFull:
            return "Inventory is full.";
        case ShopTradeFailureReason::InvalidQuantity:
            return "Invalid quantity.";
        case ShopTradeFailureReason::InvalidPlayer:
            return "Action unavailable.";
        case ShopTradeFailureReason::InvalidShop:
        case ShopTradeFailureReason::InvalidItem:
        case ShopTradeFailureReason::ItemNotSoldHere:
            return "This item cannot be purchased here.";
        case ShopTradeFailureReason::ItemNotSellable:
        case ShopTradeFailureReason::SlotMismatch:
        case ShopTradeFailureReason::InsufficientItemCount:
            return "Purchase failed.";
    }

    return "Purchase failed.";
}

[[nodiscard]] std::string formatSuccessText(std::string_view item_name, const int quantity) {
    std::string text{"Purchased "};
    text.append(item_name);
    if (quantity > 1) {
        text.push_back(' ');
        text += 'x';
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
            !constructor.RegisterArray<decltype(buy_entries_)>()) {
            spdlog::error("ShopMenuScene: 注册 ShopBuyEntryViewModel data types 失败。");
            document_controller_.unload();
            return false;
        }
        data_types_registered_ = true;
    }

    if (!constructor.Bind("shop_title", &shop_title_) ||
        !constructor.Bind("shop_greeting", &shop_greeting_) ||
        !constructor.Bind("gold_label", &gold_label_) ||
        !constructor.Bind("status_text", &status_text_) ||
        !constructor.Bind("empty_text", &empty_text_) ||
        !constructor.Bind("detail_name", &detail_name_) ||
        !constructor.Bind("detail_description", &detail_description_) ||
        !constructor.Bind("detail_price_text", &detail_price_text_) ||
        !constructor.Bind("detail_total_text", &detail_total_text_) ||
        !constructor.Bind("detail_after_gold_text", &detail_after_gold_text_) ||
        !constructor.Bind("detail_quantity_text", &detail_quantity_text_) ||
        !constructor.Bind("detail_owned_text", &detail_owned_text_) ||
        !constructor.Bind("has_buy_entries", &has_buy_entries_) ||
        !constructor.Bind("buy_enabled", &buy_enabled_) ||
        !constructor.Bind("quantity_decrease_enabled", &quantity_decrease_enabled_) ||
        !constructor.Bind("quantity_increase_enabled", &quantity_increase_enabled_) ||
        !constructor.Bind("buy_entries", &buy_entries_)) {
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
            "adjust_quantity",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                adjustQuantity(game::ui::getSingleIntArgument(arguments));
            }) ||
        !document_controller_.bindSimpleEvent(constructor, "buy_confirm", [this] { confirmBuy(); }) ||
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

void ShopMenuScene::rebuildBuyEntries() {
    buy_entry_refs_.clear();
    buy_entries_.clear();

    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!shop_data_ || !item_catalog_) {
        document_controller_.markDirty("buy_entries");
        document_controller_.markDirty("has_buy_entries");
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
}

void ShopMenuScene::refreshSelectedBuyEntry() {
    const auto* buy_entry = currentBuyEntry();
    const auto* item = currentItemData();
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
        std::to_string(currentOwnedCount()));

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

void ShopMenuScene::refreshStatusText() {
    if (!has_buy_entries_) {
        updateStringBinding(document_controller_, "status_text", status_text_, "This shop has nothing to sell.");
        return;
    }

    if (status_override_.has_value()) {
        updateStringBinding(document_controller_, "status_text", status_text_, *status_override_);
        return;
    }

    updateStringBinding(
        document_controller_,
        "status_text",
        status_text_,
        formatFailureText(active_buy_preview_.failure_reason));
}

void ShopMenuScene::refreshAll() {
    syncGoldLabel();
    rebuildBuyEntries();
    refreshSelectedBuyEntry();
    refreshBuyPreview();
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

const game::data::ItemData* ShopMenuScene::currentItemData() const {
    const auto* buy_entry = currentBuyEntry();
    if (!buy_entry || !item_catalog_) {
        return nullptr;
    }

    return item_catalog_->findItem(buy_entry->item_id_hash_);
}

int ShopMenuScene::currentOwnedCount() const {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    const auto* buy_entry = currentBuyEntry();
    if (!inventory || !buy_entry) {
        return 0;
    }

    return game::ui::countOwnedItems(*inventory, buy_entry->item_id_hash_);
}

int ShopMenuScene::currentQuantityUiMax() const {
    if (const auto* item = currentItemData()) {
        return game::ui::resolveBuyQuantityUiMax(*item);
    }
    return 1;
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
    refreshStatusText();
}

void ShopMenuScene::adjustQuantity(const int delta) {
    if (delta == 0 || !has_buy_entries_) {
        return;
    }

    const int max_quantity = currentQuantityUiMax();
    const int next_quantity = std::clamp(requested_buy_quantity_ + delta, 1, max_quantity);
    if (next_quantity == requested_buy_quantity_) {
        return;
    }

    requested_buy_quantity_ = next_quantity;
    clearStatusOverride();
    refreshSelectedBuyEntry();
    refreshBuyPreview();
    refreshStatusText();
}

void ShopMenuScene::confirmBuy() {
    const auto* buy_entry = currentBuyEntry();
    const auto* item = currentItemData();
    if (!buy_entry || !item || !shop_transaction_service_) {
        status_override_ = "No item selected.";
        refreshStatusText();
        return;
    }

    if (!active_buy_preview_.canCommit()) {
        status_override_ = formatFailureText(active_buy_preview_.failure_reason);
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
        status_override_ = formatSuccessText(item_name, result.resolved_quantity);
    } else {
        status_override_ = formatFailureText(result.failure_reason);
    }

    refreshAll();
}

bool ShopMenuScene::onMenuUpPressed() {
    if (has_buy_entries_) {
        const int entry_count = static_cast<int>(buy_entries_.size());
        selectBuyEntry((selected_buy_index_ + entry_count - 1) % entry_count);
    }
    return true;
}

bool ShopMenuScene::onMenuDownPressed() {
    if (has_buy_entries_) {
        const int entry_count = static_cast<int>(buy_entries_.size());
        selectBuyEntry((selected_buy_index_ + 1) % entry_count);
    }
    return true;
}

bool ShopMenuScene::onMenuLeftPressed() {
    adjustQuantity(-1);
    return true;
}

bool ShopMenuScene::onMenuRightPressed() {
    adjustQuantity(1);
    return true;
}

bool ShopMenuScene::onMenuConfirmPressed() {
    confirmBuy();
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
