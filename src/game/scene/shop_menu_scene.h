#pragma once

#include "game/domain/shop_transaction_service.h"
#include "game/ui/shop_menu_support.h"

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace engine::core {
enum class State;
}

namespace game::data {
class ItemCatalog;
class ShopCatalog;
struct ItemData;
struct ShopBuyEntryData;
struct ShopData;
}

namespace game::defs {
struct LanguageChangedEvent;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::scene {

class ShopMenuScene final : public engine::scene::Scene {
public:
    enum class ShopMenuMode {
        Buy,
        Sell
    };

private:
    enum class StatusOverrideKind {
        NoItemSelected,
        Failure,
        Success
    };

    struct StatusOverride {
        StatusOverrideKind kind{StatusOverrideKind::NoItemSelected};
        ShopMenuMode mode{ShopMenuMode::Buy};
        game::domain::ShopTradeFailureReason failure_reason{game::domain::ShopTradeFailureReason::None};
        std::string item_name_key{};
        std::string item_name_fallback{};
        int quantity{1};
    };

    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    std::string shop_id_{};
    const game::data::ShopCatalog* shop_catalog_{nullptr};
    game::data::ItemCatalog* item_catalog_{nullptr};
    game::domain::ShopTransactionService* shop_transaction_service_{nullptr};
    const game::runtime::LocalizationService* localization_{nullptr};
    const game::data::ShopData* shop_data_{nullptr};
    engine::core::State previous_state_{};
    bool context_pushed_{false};
    bool data_types_registered_{false};
    bool input_listeners_connected_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};
    Rml::String shop_title_{"!shop.title.default!"};
    Rml::String shop_greeting_{"!shop.greeting.default!"};
    ShopMenuMode current_mode_{ShopMenuMode::Buy};
    game::ui::ShopMenuCategory current_category_{game::ui::ShopMenuCategory::Consumable};
    game::ui::ShopMenuFocusArea current_focus_area_{game::ui::ShopMenuFocusArea::EntryList};
    bool is_buy_mode_{true};
    bool is_sell_mode_{false};
    bool is_consumable_category_{true};
    bool is_equipment_category_{false};
    bool is_mode_toggle_focused_{false};
    bool is_category_tabs_focused_{false};
    bool is_entry_list_focused_{true};
    bool is_quantity_focused_{false};
    bool is_primary_action_focused_{false};
    bool buy_entries_dirty_{true};
    bool sell_entries_dirty_{true};
    std::vector<const game::data::ShopBuyEntryData*> buy_entry_refs_{};
    std::vector<game::ui::ShopBuyEntryViewModel> buy_entries_{};
    int selected_buy_index_{0};
    int requested_buy_quantity_{1};
    game::domain::ShopBuyPreview active_buy_preview_{};
    std::vector<game::ui::ShopSellEntryViewModel> sell_entries_{};
    int selected_sell_index_{0};
    int requested_sell_quantity_{1};
    game::domain::ShopSellPreview active_sell_preview_{};
    std::optional<StatusOverride> status_override_{};
    Rml::String gold_label_{"!shop.gold_value!"};
    Rml::String status_text_{"!shop.status.focus.quantity_buy!"};
    Rml::String list_title_text_{"!shop.mode.buy!"};
    Rml::String empty_text_{"!shop.empty.buy_consumable!"};
    Rml::String detail_name_{"!shop.empty.buy_consumable!"};
    Rml::String detail_description_{};
    Rml::String detail_price_text_{"-"};
    Rml::String detail_total_text_{"-"};
    Rml::String detail_after_gold_text_{"-"};
    Rml::String detail_quantity_text_{"x0"};
    Rml::String detail_owned_label_{"!shop.detail.owned!"};
    Rml::String detail_owned_text_{"0"};
    bool has_buy_entries_{false};
    bool has_sell_entries_{false};
    bool has_consumable_entries_{false};
    bool has_equipment_entries_{false};
    bool buy_enabled_{false};
    bool sell_enabled_{false};
    bool primary_action_enabled_{false};
    bool quantity_decrease_enabled_{false};
    bool quantity_increase_enabled_{false};
    Rml::String primary_action_text_{"!shop.mode.buy!"};

public:
    ShopMenuScene(std::string_view name,
                  engine::core::Context& context,
                  entt::registry& game_registry,
                  entt::entity player,
                  std::string shop_id,
                  const game::data::ShopCatalog* shop_catalog,
                  game::data::ItemCatalog* item_catalog,
                  game::domain::ShopTransactionService* shop_transaction_service);
    ~ShopMenuScene() override;

    [[nodiscard]] bool init() override;
    void clean() override;
    [[nodiscard]] engine::scene::SceneUiCoverage uiCoverage() const override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void connectRuntimeListeners();
    void disconnectRuntimeListeners();
    void syncShopHeaderBindings();
    void syncGoldLabel();
    void syncModeBindings();
    void syncCategoryBindings();
    void syncFocusBindings();
    void normalizeFocusArea();
    void markTradeListsDirty();
    void rebuildBuyEntries();
    void rebuildSellEntries();
    void refreshSelectedBuyEntry();
    void refreshSelectedSellEntry();
    void refreshBuyPreview();
    void refreshSellPreview();
    void refreshStatusText();
    void refreshAll();
    void clearStatusOverride();
    [[nodiscard]] std::string formatStatusOverride(const StatusOverride& status) const;
    [[nodiscard]] std::string localizeCatalogText(std::string_view key_or_text, std::string_view fallback) const;
    [[nodiscard]] std::string itemNameKey(const game::data::ItemData& item) const;
    [[nodiscard]] std::string itemNameFallback(const game::data::ItemData& item, std::string_view fallback_id) const;
    [[nodiscard]] std::string localizedItemName(const game::data::ItemData& item, std::string_view fallback_id) const;
    [[nodiscard]] const game::data::ShopBuyEntryData* currentBuyEntry() const;
    [[nodiscard]] const game::data::ItemData* currentBuyItemData() const;
    [[nodiscard]] const game::ui::ShopSellEntryViewModel* currentSellEntry() const;
    [[nodiscard]] const game::data::ItemData* currentSellItemData() const;
    [[nodiscard]] entt::id_type currentSellItemId() const;
    [[nodiscard]] int currentSellSlotIndex() const;
    [[nodiscard]] int currentBuyOwnedCount() const;
    [[nodiscard]] int currentSellSlotCount() const;
    [[nodiscard]] bool hasCurrentEntries() const;
    [[nodiscard]] bool hasEntriesForCategory(ShopMenuMode mode, game::ui::ShopMenuCategory category) const;
    [[nodiscard]] int currentQuantityUiMax() const;
    [[nodiscard]] bool isCurrentQuantityAdjustable() const;
    [[nodiscard]] game::ui::ShopMenuNavigationState makeNavigationState() const;
    void applyFocusArea(game::ui::ShopMenuFocusArea next_focus_area);
    void applyNavigationDecision(const game::ui::ShopMenuNavigationDecision& decision);
    [[nodiscard]] int resolveClickedEntryIndex(const Rml::Event& event) const;
    void selectBuyEntry(int index);
    void selectSellEntry(int index);
    void adjustQuantity(int delta);
    void switchMode(ShopMenuMode next_mode);
    void switchCategory(game::ui::ShopMenuCategory next_category);
    void confirmBuy();
    void confirmSell();
    [[nodiscard]] bool onMenuUpPressed();
    [[nodiscard]] bool onMenuDownPressed();
    [[nodiscard]] bool onMenuLeftPressed();
    [[nodiscard]] bool onMenuRightPressed();
    [[nodiscard]] bool onMenuConfirmPressed();
    [[nodiscard]] bool onMenuCancelPressed();
    void onLanguageChanged(const game::defs::LanguageChangedEvent& event);
    void onClose();
};

} // namespace game::scene
