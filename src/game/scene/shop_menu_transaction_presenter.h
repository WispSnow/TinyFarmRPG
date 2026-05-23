#pragma once

#include "game/domain/shop_transaction_service.h"
#include "game/ui/shop_menu_support.h"

#include <string>
#include <string_view>

namespace game::scene {

enum class ShopTradeMode {
    Buy,
    Sell
};

/// @brief Formats shop transaction text shown by ShopMenuScene.
class ShopMenuTransactionPresenter final {
public:
    [[nodiscard]] static std::string formatGoldLabel(int gold);
    [[nodiscard]] static std::string formatGoldValue(int gold);
    [[nodiscard]] static std::string formatQuantityText(int quantity);
    [[nodiscard]] static std::string_view defaultEmptyText(ShopTradeMode mode, game::ui::ShopMenuCategory category);
    [[nodiscard]] static std::string formatFocusStatus(ShopTradeMode mode,
                                                       game::ui::ShopMenuFocusArea focus_area,
                                                       bool quantity_adjustable);
    [[nodiscard]] static std::string formatFailureText(ShopTradeMode mode,
                                                       game::domain::ShopTradeFailureReason failure_reason);
    [[nodiscard]] static std::string formatSuccessText(ShopTradeMode mode, std::string_view item_name, int quantity);
};

} // namespace game::scene
