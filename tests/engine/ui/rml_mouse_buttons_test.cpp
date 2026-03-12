#include "engine/ui/rmlui/rml_mouse_buttons.h"

#include <gtest/gtest.h>

namespace {

TEST(RmlMouseButtonsTest, UsesDomStyleButtonOrdering) {
    EXPECT_EQ(engine::ui::rmlui::kPrimaryMouseButton, 0);
    EXPECT_EQ(engine::ui::rmlui::kSecondaryMouseButton, 1);
    EXPECT_EQ(engine::ui::rmlui::kMiddleMouseButton, 2);
}

TEST(RmlMouseButtonsTest, HelperPredicatesMatchExpectedButtons) {
    EXPECT_TRUE(engine::ui::rmlui::isPrimaryMouseButton(0));
    EXPECT_FALSE(engine::ui::rmlui::isPrimaryMouseButton(1));

    EXPECT_TRUE(engine::ui::rmlui::isSecondaryMouseButton(1));
    EXPECT_FALSE(engine::ui::rmlui::isSecondaryMouseButton(2));

    EXPECT_TRUE(engine::ui::rmlui::isMiddleMouseButton(2));
    EXPECT_FALSE(engine::ui::rmlui::isMiddleMouseButton(0));
}

} // namespace
