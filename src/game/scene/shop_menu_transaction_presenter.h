#pragma once

#include "game/domain/shop_transaction_service.h"
#include "game/ui/shop_menu_support.h"

#include <string>
#include <string_view>

namespace game::runtime {
class LocalizationService;
}

namespace game::scene {

enum class ShopTradeMode {
    Buy,
    Sell
};

/// @brief Formats shop transaction text shown by ShopMenuScene.
class ShopMenuTransactionPresenter final {
public:
    [[nodiscard]] static std::string formatGoldLabel(const game::runtime::LocalizationService* localization, int gold);
    [[nodiscard]] static std::string formatGoldValue(const game::runtime::LocalizationService* localization, int gold);
    [[nodiscard]] static std::string formatQuantityText(const game::runtime::LocalizationService* localization,
                                                        int quantity);
    [[nodiscard]] static std::string defaultEmptyText(const game::runtime::LocalizationService* localization,
                                                      ShopTradeMode mode,
                                                      game::ui::ShopMenuCategory category);
    [[nodiscard]] static std::string formatOwnedLabel(const game::runtime::LocalizationService* localization,
                                                      ShopTradeMode mode);
    [[nodiscard]] static std::string formatFocusStatus(ShopTradeMode mode,
                                                       game::ui::ShopMenuFocusArea focus_area,
                                                       bool quantity_adjustable,
                                                       const game::runtime::LocalizationService* localization);
    [[nodiscard]] static std::string formatFailureText(ShopTradeMode mode,
                                                       game::domain::ShopTradeFailureReason failure_reason,
                                                       const game::runtime::LocalizationService* localization);
    [[nodiscard]] static std::string formatNoItemSelected(const game::runtime::LocalizationService* localization);
    [[nodiscard]] static std::string formatSuccessText(ShopTradeMode mode,
                                                       std::string_view item_name,
                                                       int quantity,
                                                       const game::runtime::LocalizationService* localization);
};

} // namespace game::scene
