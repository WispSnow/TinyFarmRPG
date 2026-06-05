#include "game/scene/shop_menu_scene.h"

#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/data/shop_data.h"
#include "game/defs/options_events.h"
#include "game/runtime/localization_service.h"
#include "game/runtime/service_lookup.h"
#include "game/scene/shop_menu_transaction_presenter.h"
#include "game/scene/shop_trade_list_builder.h"
#include "game/ui/localized_text.h"
#include "game/ui/slot_grid_support.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"

#include <RmlUi/Core/Element.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <utility>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/shop_menu.rml";
constexpr std::string_view MODEL_NAME = "shop_menu";

using ShopMenuMode = game::scene::ShopMenuScene::ShopMenuMode;
using ShopMenuCategory = game::ui::ShopMenuCategory;
using ShopMenuFocusArea = game::ui::ShopMenuFocusArea;
using ShopMenuNavigationDecision = game::ui::ShopMenuNavigationDecision;
using ShopMenuNavigationInput = game::ui::ShopMenuNavigationInput;
using ShopMenuNavigationState = game::ui::ShopMenuNavigationState;
using ShopTransactionPresenter = game::scene::ShopMenuTransactionPresenter;
using ShopTradeMode = game::scene::ShopTradeMode;
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

[[nodiscard]] ShopTradeMode toTradeMode(const ShopMenuMode mode) {
    return mode == ShopMenuMode::Buy ? ShopTradeMode::Buy : ShopTradeMode::Sell;
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
    localization_ = game::runtime::findLocalizationService(game_registry_);

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
    spdlog::info("ShopMenuScene: opened shop_id='{}'.", shop_id_);
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

engine::scene::SceneUiCoverage ShopMenuScene::uiCoverage() const {
    return engine::scene::SceneUiCoverage::HideUnderlyingSceneUi;
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
        !constructor.Bind("is_consumable_category", &is_consumable_category_) ||
        !constructor.Bind("is_equipment_category", &is_equipment_category_) ||
        !constructor.Bind("is_mode_toggle_focused", &is_mode_toggle_focused_) ||
        !constructor.Bind("is_category_tabs_focused", &is_category_tabs_focused_) ||
        !constructor.Bind("is_entry_list_focused", &is_entry_list_focused_) ||
        !constructor.Bind("is_quantity_focused", &is_quantity_focused_) ||
        !constructor.Bind("is_primary_action_focused", &is_primary_action_focused_) ||
        !constructor.Bind("has_buy_entries", &has_buy_entries_) ||
        !constructor.Bind("has_sell_entries", &has_sell_entries_) ||
        !constructor.Bind("has_consumable_entries", &has_consumable_entries_) ||
        !constructor.Bind("has_equipment_entries", &has_equipment_entries_) ||
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
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) {
                selectBuyEntry(resolveClickedEntryIndex(event));
            }) ||
        !document_controller_.bindEvent(
            constructor,
            "sell_entry_select",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) {
                selectSellEntry(resolveClickedEntryIndex(event));
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
            "switch_category_consumable",
            [this] { switchCategory(ShopMenuCategory::Consumable); }) ||
        !document_controller_.bindSimpleEvent(
            constructor,
            "switch_category_equipment",
            [this] { switchCategory(ShopMenuCategory::Equipment); }) ||
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
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().connect<&ShopMenuScene::onLanguageChanged>(this);
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
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().disconnect<&ShopMenuScene::onLanguageChanged>(this);
    input_listeners_connected_ = false;
}

void ShopMenuScene::syncShopHeaderBindings() {
    updateStringBinding(
        document_controller_,
        "shop_title",
        shop_title_,
        localizeCatalogText(
            shop_data_ ? shop_data_->title_ : std::string_view{},
            game::ui::localizeTextOrFallback(localization_, "shop.title.default", "Shop")));
    updateStringBinding(
        document_controller_,
        "shop_greeting",
        shop_greeting_,
        localizeCatalogText(
            shop_data_ ? shop_data_->greeting_ : std::string_view{},
            game::ui::localizeTextOrFallback(localization_, "shop.greeting.default", "Welcome.")));
}

void ShopMenuScene::syncGoldLabel() {
    const auto* wallet = game_registry_.try_get<game::component::PlayerWalletComponent>(player_);
    updateStringBinding(
        document_controller_,
        "gold_label",
        gold_label_,
        ShopTransactionPresenter::formatGoldLabel(localization_, wallet ? wallet->gold_ : 0));
}

void ShopMenuScene::syncModeBindings() {
    const bool is_buy_mode = current_mode_ == ShopMenuMode::Buy;
    updateBoolBinding(document_controller_, "is_buy_mode", is_buy_mode_, is_buy_mode);
    updateBoolBinding(document_controller_, "is_sell_mode", is_sell_mode_, !is_buy_mode);
    updateStringBinding(
        document_controller_,
        "list_title_text",
        list_title_text_,
        game::ui::localizeTextOrFallback(
            localization_,
            is_buy_mode ? "shop.mode.buy" : "shop.mode.sell",
            is_buy_mode ? "Buy" : "Sell"));
    updateStringBinding(
        document_controller_,
        "detail_owned_label",
        detail_owned_label_,
        ShopTransactionPresenter::formatOwnedLabel(localization_, toTradeMode(current_mode_)));
    updateStringBinding(
        document_controller_,
        "primary_action_text",
        primary_action_text_,
        game::ui::localizeTextOrFallback(
            localization_,
            is_buy_mode ? "shop.mode.buy" : "shop.mode.sell",
            is_buy_mode ? "Buy" : "Sell"));
    // This mirrors the active mode's preview validity instead of introducing a third independent action state.
    updateBoolBinding(
        document_controller_,
        "primary_action_enabled",
        primary_action_enabled_,
        is_buy_mode ? buy_enabled_ : sell_enabled_);
}

void ShopMenuScene::syncCategoryBindings() {
    updateBoolBinding(
        document_controller_,
        "is_consumable_category",
        is_consumable_category_,
        current_category_ == ShopMenuCategory::Consumable);
    updateBoolBinding(
        document_controller_,
        "is_equipment_category",
        is_equipment_category_,
        current_category_ == ShopMenuCategory::Equipment);
    updateBoolBinding(
        document_controller_,
        "has_consumable_entries",
        has_consumable_entries_,
        hasEntriesForCategory(current_mode_, ShopMenuCategory::Consumable));
    updateBoolBinding(
        document_controller_,
        "has_equipment_entries",
        has_equipment_entries_,
        hasEntriesForCategory(current_mode_, ShopMenuCategory::Equipment));
    updateStringBinding(
        document_controller_,
        "empty_text",
        empty_text_,
        ShopTransactionPresenter::defaultEmptyText(localization_, toTradeMode(current_mode_), current_category_));
}

void ShopMenuScene::syncFocusBindings() {
    updateBoolBinding(
        document_controller_,
        "is_mode_toggle_focused",
        is_mode_toggle_focused_,
        current_focus_area_ == ShopMenuFocusArea::ModeToggle);
    updateBoolBinding(
        document_controller_,
        "is_category_tabs_focused",
        is_category_tabs_focused_,
        current_focus_area_ == ShopMenuFocusArea::CategoryTabs);
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
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    auto snapshot = ShopTradeListBuilder::buildBuyList(
        shop_data_,
        item_catalog_,
        inventory,
        localization_,
        current_category_,
        selected_buy_index_,
        shop_id_);
    buy_entry_refs_ = std::move(snapshot.entry_refs);
    buy_entries_ = std::move(snapshot.entries);
    selected_buy_index_ = snapshot.selected_index;

    updateBoolBinding(document_controller_, "has_buy_entries", has_buy_entries_, !buy_entries_.empty());
    document_controller_.markDirty("buy_entries");
    buy_entries_dirty_ = false;
}

void ShopMenuScene::rebuildSellEntries() {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    auto snapshot = ShopTradeListBuilder::buildSellList(
        inventory,
        item_catalog_,
        shop_catalog_,
        localization_,
        current_category_,
        selected_sell_index_);
    sell_entries_ = std::move(snapshot.entries);
    selected_sell_index_ = snapshot.selected_index;

    updateBoolBinding(document_controller_, "has_sell_entries", has_sell_entries_, !sell_entries_.empty());
    document_controller_.markDirty("sell_entries");
    sell_entries_dirty_ = false;
}

void ShopMenuScene::refreshSelectedBuyEntry() {
    const auto* buy_entry = currentBuyEntry();
    const auto* item = currentBuyItemData();
    if (!buy_entry || !item) {
        requested_buy_quantity_ = 1;
        updateStringBinding(
            document_controller_,
            "detail_name",
            detail_name_,
            ShopTransactionPresenter::defaultEmptyText(localization_, ShopTradeMode::Buy, current_category_));
        updateStringBinding(
            document_controller_,
            "detail_description",
            detail_description_,
            game::ui::localizeTextOrFallback(
                localization_,
                "shop.detail.no_purchasable_goods",
                "This shop has no purchasable goods in this category."));
        updateStringBinding(
            document_controller_,
            "detail_quantity_text",
            detail_quantity_text_,
            ShopTransactionPresenter::formatQuantityText(localization_, 0));
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
        localizedItemName(*item, buy_entry->item_id_));
    updateStringBinding(
        document_controller_,
        "detail_description",
        detail_description_,
        localizeCatalogText(item->description_, item->description_));
    updateStringBinding(
        document_controller_,
        "detail_quantity_text",
        detail_quantity_text_,
        ShopTransactionPresenter::formatQuantityText(localization_, requested_buy_quantity_));
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
        updateStringBinding(
            document_controller_,
            "detail_name",
            detail_name_,
            ShopTransactionPresenter::defaultEmptyText(localization_, ShopTradeMode::Sell, current_category_));
        updateStringBinding(
            document_controller_,
            "detail_description",
            detail_description_,
            game::ui::localizeTextOrFallback(
                localization_,
                "shop.detail.nothing_to_sell",
                "You have nothing to sell in this category."));
        updateStringBinding(
            document_controller_,
            "detail_quantity_text",
            detail_quantity_text_,
            ShopTransactionPresenter::formatQuantityText(localization_, 0));
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
        localizedItemName(*item, item->id_str_));
    updateStringBinding(
        document_controller_,
        "detail_description",
        detail_description_,
        localizeCatalogText(item->description_, item->description_));
    updateStringBinding(
        document_controller_,
        "detail_quantity_text",
        detail_quantity_text_,
        ShopTransactionPresenter::formatQuantityText(localization_, requested_sell_quantity_));
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
        ShopTransactionPresenter::formatGoldValue(localization_, active_buy_preview_.unit_price));
    updateStringBinding(
        document_controller_,
        "detail_total_text",
        detail_total_text_,
        ShopTransactionPresenter::formatGoldValue(localization_, active_buy_preview_.total_price));
    updateStringBinding(
        document_controller_,
        "detail_after_gold_text",
        detail_after_gold_text_,
        ShopTransactionPresenter::formatGoldValue(localization_, active_buy_preview_.final_gold_after));
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
            ShopTransactionPresenter::formatGoldValue(localization_, active_sell_preview_.final_gold_after));
        updateBoolBinding(document_controller_, "sell_enabled", sell_enabled_, false);
        return;
    }

    updateStringBinding(
        document_controller_,
        "detail_price_text",
        detail_price_text_,
        ShopTransactionPresenter::formatGoldValue(localization_, active_sell_preview_.unit_price));
    updateStringBinding(
        document_controller_,
        "detail_total_text",
        detail_total_text_,
        ShopTransactionPresenter::formatGoldValue(localization_, active_sell_preview_.total_price));
    updateStringBinding(
        document_controller_,
        "detail_after_gold_text",
        detail_after_gold_text_,
        ShopTransactionPresenter::formatGoldValue(localization_, active_sell_preview_.final_gold_after));
    updateBoolBinding(document_controller_, "sell_enabled", sell_enabled_, active_sell_preview_.canCommit());
}

void ShopMenuScene::refreshStatusText() {
    if (status_override_.has_value()) {
        updateStringBinding(document_controller_, "status_text", status_text_, formatStatusOverride(*status_override_));
        return;
    }

    if (!hasCurrentEntries()) {
        updateStringBinding(
            document_controller_,
            "status_text",
            status_text_,
            ShopTransactionPresenter::defaultEmptyText(localization_, toTradeMode(current_mode_), current_category_));
        return;
    }

    const auto failure_reason =
        current_mode_ == ShopMenuMode::Buy ? active_buy_preview_.failure_reason : active_sell_preview_.failure_reason;
    if (failure_reason == game::domain::ShopTradeFailureReason::None) {
        updateStringBinding(
            document_controller_,
            "status_text",
            status_text_,
            ShopTransactionPresenter::formatFocusStatus(
                toTradeMode(current_mode_),
                current_focus_area_,
                isCurrentQuantityAdjustable(),
                localization_));
        return;
    }

    updateStringBinding(
        document_controller_,
        "status_text",
        status_text_,
        ShopTransactionPresenter::formatFailureText(toTradeMode(current_mode_), failure_reason, localization_));
}

void ShopMenuScene::refreshAll() {
    syncShopHeaderBindings();
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
    syncCategoryBindings();
    syncFocusBindings();
    refreshStatusText();
}

void ShopMenuScene::clearStatusOverride() {
    status_override_.reset();
}

std::string ShopMenuScene::formatStatusOverride(const StatusOverride& status) const {
    switch (status.kind) {
        case StatusOverrideKind::NoItemSelected:
            return ShopTransactionPresenter::formatNoItemSelected(localization_);
        case StatusOverrideKind::Failure:
            return ShopTransactionPresenter::formatFailureText(
                toTradeMode(status.mode),
                status.failure_reason,
                localization_);
        case StatusOverrideKind::Success: {
            const std::string item_name = localizeCatalogText(status.item_name_key, status.item_name_fallback);
            return ShopTransactionPresenter::formatSuccessText(
                toTradeMode(status.mode),
                item_name,
                status.quantity,
                localization_);
        }
    }

    return {};
}

std::string ShopMenuScene::localizeCatalogText(const std::string_view key_or_text, const std::string_view fallback) const {
    return key_or_text.empty() ? std::string{fallback} : game::ui::tryLocalize(localization_, key_or_text);
}

std::string ShopMenuScene::itemNameKey(const game::data::ItemData& item) const {
    return item.display_name_.empty() ? item.id_str_ : item.display_name_;
}

std::string ShopMenuScene::itemNameFallback(const game::data::ItemData& item, const std::string_view fallback_id) const {
    if (!item.display_name_.empty()) {
        return item.display_name_;
    }
    if (!item.id_str_.empty()) {
        return item.id_str_;
    }
    return std::string{fallback_id};
}

std::string ShopMenuScene::localizedItemName(const game::data::ItemData& item, const std::string_view fallback_id) const {
    return localizeCatalogText(itemNameKey(item), itemNameFallback(item, fallback_id));
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

bool ShopMenuScene::hasEntriesForCategory(const ShopMenuMode mode, const ShopMenuCategory category) const {
    if (mode == ShopMenuMode::Buy) {
        return ShopTradeListBuilder::hasBuyEntriesForCategory(shop_data_, item_catalog_, category);
    }

    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    return ShopTradeListBuilder::hasSellEntriesForCategory(inventory, item_catalog_, category);
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
        .current_category = current_category_,
        .focus_area = current_focus_area_,
        .has_consumable_entries = has_consumable_entries_,
        .has_equipment_entries = has_equipment_entries_,
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
    const bool category_changed = decision.switch_category && decision.next_category != current_category_;
    const bool selection_changed = decision.entry_delta != 0 && hasCurrentEntries();
    const bool quantity_changed = decision.quantity_delta != 0 && hasCurrentEntries();
    if (mode_changed || category_changed || decision.next_focus_area != current_focus_area_ || selection_changed ||
        quantity_changed) {
        clearStatusOverride();
    }

    if (mode_changed) {
        switchMode(decision.next_is_buy_mode ? ShopMenuMode::Buy : ShopMenuMode::Sell);
    }

    if (category_changed) {
        switchCategory(decision.next_category);
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

int ShopMenuScene::resolveClickedEntryIndex(const Rml::Event& event) const {
    const Rml::Element* element = event.GetCurrentElement();
    if (!element) {
        element = event.GetTargetElement();
    }

    return element ? element->GetAttribute<int>("data-shop-index", -1) : -1;
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
    if (current_mode_ == ShopMenuMode::Buy) {
        buy_entries_dirty_ = true;
    } else {
        sell_entries_dirty_ = true;
    }
    if (current_focus_area_ != ShopMenuFocusArea::ModeToggle) {
        current_focus_area_ =
            game::ui::resolvePreferredShopMenuFocus(hasEntriesForCategory(current_mode_, current_category_));
    }
    clearStatusOverride();
    refreshAll();
}

void ShopMenuScene::switchCategory(const ShopMenuCategory next_category) {
    if (current_category_ == next_category) {
        return;
    }

    current_category_ = next_category;
    if (current_mode_ == ShopMenuMode::Buy) {
        selected_buy_index_ = 0;
        requested_buy_quantity_ = 1;
    } else {
        selected_sell_index_ = 0;
        requested_sell_quantity_ = 1;
    }

    markTradeListsDirty();
    clearStatusOverride();
    refreshAll();
}

void ShopMenuScene::confirmBuy() {
    const auto* buy_entry = currentBuyEntry();
    const auto* item = currentBuyItemData();
    if (!buy_entry || !item || !shop_transaction_service_) {
        status_override_ = StatusOverride{.kind = StatusOverrideKind::NoItemSelected, .mode = ShopMenuMode::Buy};
        refreshStatusText();
        return;
    }

    if (!active_buy_preview_.canCommit()) {
        status_override_ = StatusOverride{
            .kind = StatusOverrideKind::Failure,
            .mode = ShopMenuMode::Buy,
            .failure_reason = active_buy_preview_.failure_reason};
        refreshStatusText();
        spdlog::info(
            "ShopMenuScene: buy failed shop_id='{}' reason={}.",
            shop_id_,
            static_cast<int>(active_buy_preview_.failure_reason));
        return;
    }

    const auto result = shop_transaction_service_->commitBuy(
        player_,
        shop_id_,
        buy_entry->item_id_hash_,
        requested_buy_quantity_);
    if (result.completed()) {
        requested_buy_quantity_ = 1;
        status_override_ = StatusOverride{
            .kind = StatusOverrideKind::Success,
            .mode = ShopMenuMode::Buy,
            .item_name_key = itemNameKey(*item),
            .item_name_fallback = itemNameFallback(*item, buy_entry->item_id_),
            .quantity = result.resolved_quantity};
        spdlog::info(
            "ShopMenuScene: buy completed shop_id='{}' item_id='{}' quantity={}.",
            shop_id_,
            buy_entry->item_id_,
            result.resolved_quantity);
    } else {
        status_override_ = StatusOverride{
            .kind = StatusOverrideKind::Failure,
            .mode = ShopMenuMode::Buy,
            .failure_reason = result.failure_reason};
        spdlog::info(
            "ShopMenuScene: buy failed shop_id='{}' reason={}.",
            shop_id_,
            static_cast<int>(result.failure_reason));
    }

    markTradeListsDirty();
    refreshAll();
}

void ShopMenuScene::confirmSell() {
    const auto* item = currentSellItemData();
    const entt::id_type item_id = currentSellItemId();
    const int slot_index = currentSellSlotIndex();
    if (!item || item_id == entt::null || slot_index < 0 || !shop_transaction_service_) {
        status_override_ = StatusOverride{.kind = StatusOverrideKind::NoItemSelected, .mode = ShopMenuMode::Sell};
        refreshStatusText();
        return;
    }

    if (!active_sell_preview_.canCommit()) {
        status_override_ = StatusOverride{
            .kind = StatusOverrideKind::Failure,
            .mode = ShopMenuMode::Sell,
            .failure_reason = active_sell_preview_.failure_reason};
        refreshStatusText();
        spdlog::info(
            "ShopMenuScene: sell failed shop_id='{}' reason={}.",
            shop_id_,
            static_cast<int>(active_sell_preview_.failure_reason));
        return;
    }

    const auto result = shop_transaction_service_->commitSell(
        player_,
        item_id,
        requested_sell_quantity_,
        slot_index);
    if (result.completed()) {
        requested_sell_quantity_ = 1;
        status_override_ = StatusOverride{
            .kind = StatusOverrideKind::Success,
            .mode = ShopMenuMode::Sell,
            .item_name_key = itemNameKey(*item),
            .item_name_fallback = itemNameFallback(*item, item->id_str_),
            .quantity = result.resolved_quantity};
        spdlog::info(
            "ShopMenuScene: sell completed shop_id='{}' item_id='{}' quantity={}.",
            shop_id_,
            item->id_str_,
            result.resolved_quantity);
    } else {
        status_override_ = StatusOverride{
            .kind = StatusOverrideKind::Failure,
            .mode = ShopMenuMode::Sell,
            .failure_reason = result.failure_reason};
        spdlog::info(
            "ShopMenuScene: sell failed shop_id='{}' reason={}.",
            shop_id_,
            static_cast<int>(result.failure_reason));
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

void ShopMenuScene::onLanguageChanged(const game::defs::LanguageChangedEvent&) {
    markTradeListsDirty();
    refreshAll();
}

void ShopMenuScene::onClose() {
    requestPopScene();
}

} // namespace game::scene
