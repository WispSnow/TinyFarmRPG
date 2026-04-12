// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "game/ui/shop_menu_support.h"

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::ui {
namespace {

TEST(ShopMenuNavigationTest, PreferredFocusFallsBackToModeToggleWhenCurrentModeIsEmpty) {
    EXPECT_EQ(resolvePreferredShopMenuFocus(true), ShopMenuFocusArea::EntryList);
    EXPECT_EQ(resolvePreferredShopMenuFocus(false), ShopMenuFocusArea::ModeToggle);
}

TEST(ShopMenuNavigationTest, ModeToggleLeftRightSwitchesTowardBuyAndSell) {
    ShopMenuNavigationState buy_state{};
    buy_state.is_buy_mode = true;
    buy_state.focus_area = ShopMenuFocusArea::ModeToggle;
    buy_state.has_buy_entries = true;
    buy_state.has_sell_entries = true;

    const auto to_sell = resolveShopMenuNavigation(buy_state, ShopMenuNavigationInput::Right);
    EXPECT_TRUE(to_sell.switch_mode);
    EXPECT_FALSE(to_sell.next_is_buy_mode);
    EXPECT_EQ(to_sell.next_focus_area, ShopMenuFocusArea::ModeToggle);

    ShopMenuNavigationState sell_state = buy_state;
    sell_state.is_buy_mode = false;
    const auto to_buy = resolveShopMenuNavigation(sell_state, ShopMenuNavigationInput::Left);
    EXPECT_TRUE(to_buy.switch_mode);
    EXPECT_TRUE(to_buy.next_is_buy_mode);
    EXPECT_EQ(to_buy.next_focus_area, ShopMenuFocusArea::ModeToggle);

    const auto enter_list = resolveShopMenuNavigation(buy_state, ShopMenuNavigationInput::Down);
    EXPECT_FALSE(enter_list.switch_mode);
    EXPECT_EQ(enter_list.next_focus_area, ShopMenuFocusArea::EntryList);
}

TEST(ShopMenuNavigationTest, EntryListUsesDirectionalInputForSelectionAndFocusTransfer) {
    ShopMenuNavigationState state{};
    state.is_buy_mode = true;
    state.focus_area = ShopMenuFocusArea::EntryList;
    state.has_buy_entries = true;
    state.has_sell_entries = false;
    state.quantity_adjustable = true;

    const auto previous = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Up);
    EXPECT_EQ(previous.entry_delta, -1);
    EXPECT_EQ(previous.next_focus_area, ShopMenuFocusArea::EntryList);

    const auto next = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Down);
    EXPECT_EQ(next.entry_delta, 1);
    EXPECT_EQ(next.next_focus_area, ShopMenuFocusArea::EntryList);

    const auto to_toggle = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Left);
    EXPECT_EQ(to_toggle.next_focus_area, ShopMenuFocusArea::ModeToggle);

    const auto to_quantity = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Right);
    EXPECT_EQ(to_quantity.next_focus_area, ShopMenuFocusArea::Quantity);

    const auto to_primary = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Confirm);
    EXPECT_EQ(to_primary.next_focus_area, ShopMenuFocusArea::PrimaryAction);
}

TEST(ShopMenuNavigationTest, QuantityAreaAdjustsStackedItemsAndEscalatesToPrimaryAction) {
    ShopMenuNavigationState state{};
    state.is_buy_mode = true;
    state.focus_area = ShopMenuFocusArea::Quantity;
    state.has_buy_entries = true;
    state.quantity_adjustable = true;

    const auto decrease = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Left);
    EXPECT_EQ(decrease.quantity_delta, -1);

    const auto increase = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Right);
    EXPECT_EQ(increase.quantity_delta, 1);

    const auto to_entry_list = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Up);
    EXPECT_EQ(to_entry_list.next_focus_area, ShopMenuFocusArea::EntryList);

    const auto down_to_primary = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Down);
    EXPECT_EQ(down_to_primary.next_focus_area, ShopMenuFocusArea::PrimaryAction);

    const auto to_primary = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Confirm);
    EXPECT_EQ(to_primary.next_focus_area, ShopMenuFocusArea::PrimaryAction);

    state.quantity_adjustable = false;
    const auto fixed_quantity = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Right);
    EXPECT_EQ(fixed_quantity.quantity_delta, 0);
    EXPECT_EQ(fixed_quantity.next_focus_area, ShopMenuFocusArea::Quantity);
}

TEST(ShopMenuNavigationTest, PrimaryActionConfirmSubmitsTradeAndCanReturnToEarlierAreas) {
    ShopMenuNavigationState state{};
    state.is_buy_mode = false;
    state.focus_area = ShopMenuFocusArea::PrimaryAction;
    state.has_buy_entries = true;
    state.has_sell_entries = true;

    const auto submit = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Confirm);
    EXPECT_TRUE(submit.confirm_trade);
    EXPECT_EQ(submit.next_focus_area, ShopMenuFocusArea::PrimaryAction);

    const auto to_quantity = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Up);
    EXPECT_EQ(to_quantity.next_focus_area, ShopMenuFocusArea::Quantity);

    const auto to_entry_list = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Left);
    EXPECT_EQ(to_entry_list.next_focus_area, ShopMenuFocusArea::EntryList);
}

TEST(ShopMenuNavigationTest, EmptyCurrentModeStaysOnModeToggleUntilEntriesExist) {
    ShopMenuNavigationState state{};
    state.is_buy_mode = false;
    state.focus_area = ShopMenuFocusArea::ModeToggle;
    state.has_buy_entries = true;
    state.has_sell_entries = false;

    const auto try_enter_empty_list = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Confirm);
    EXPECT_EQ(try_enter_empty_list.next_focus_area, ShopMenuFocusArea::ModeToggle);
    EXPECT_FALSE(try_enter_empty_list.switch_mode);

    state.focus_area = ShopMenuFocusArea::EntryList;
    const auto recover_to_toggle = resolveShopMenuNavigation(state, ShopMenuNavigationInput::Left);
    EXPECT_EQ(recover_to_toggle.next_focus_area, ShopMenuFocusArea::ModeToggle);
    EXPECT_EQ(recover_to_toggle.entry_delta, 0);
}

TEST(ShopMenuNavigationSourceTest, ShopMenuSceneRoutesMenuInputThroughNavigationHelperAndFocusBindings) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_scene.cpp").lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/shop_menu.rml").lexically_normal();
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/shop_menu.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    const std::string rml = test_source_utils::readTextFile(rml_path);
    const std::string rcss = test_source_utils::readTextFile(rcss_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(rml.empty());
    ASSERT_FALSE(rcss.empty());

    EXPECT_NE(header.find("ShopMenuFocusArea current_focus_area_"), std::string::npos);
    EXPECT_NE(header.find("syncFocusBindings()"), std::string::npos);
    EXPECT_NE(header.find("makeNavigationState() const"), std::string::npos);
    EXPECT_NE(header.find("applyNavigationDecision"), std::string::npos);
    EXPECT_NE(source.find("formatFocusStatus"), std::string::npos);
    EXPECT_NE(source.find("resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Up)"), std::string::npos);
    EXPECT_NE(source.find("resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Down)"), std::string::npos);
    EXPECT_NE(source.find("resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Left)"), std::string::npos);
    EXPECT_NE(source.find("resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Right)"), std::string::npos);
    EXPECT_NE(source.find("resolveShopMenuNavigation(makeNavigationState(), ShopMenuNavigationInput::Confirm)"), std::string::npos);
    EXPECT_NE(source.find("syncFocusBindings();"), std::string::npos);
    EXPECT_NE(source.find("current_focus_area_ != ShopMenuFocusArea::ModeToggle"), std::string::npos);

    EXPECT_NE(rml.find("data-class-focused=\"is_mode_toggle_focused\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-focused=\"is_entry_list_focused\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-focused=\"is_quantity_focused\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-focused=\"is_primary_action_focused\""), std::string::npos);

    EXPECT_NE(rcss.find("#shop-mode-toggle.focused"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-buy-list.focused"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-sell-list.focused"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-quantity-controls.focused"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-action-button.focused"), std::string::npos);
}

} // namespace
} // namespace game::ui
// NOLINTEND
