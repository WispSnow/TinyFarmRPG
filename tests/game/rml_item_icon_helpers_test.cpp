#include <gtest/gtest.h>

#include <entt/entity/fwd.hpp>

#include "game/ui/rml_item_icon_helpers.h"

namespace game::ui {
namespace {

TEST(RmlItemIconHelpersTest, HasDecoratorTreatsEmptyAndNoneAsNoDecorator) {
    EXPECT_FALSE(hasDecorator({}));
    EXPECT_FALSE(hasDecorator(kNoDecorator));
    EXPECT_TRUE(hasDecorator("image(item-tools-hoe)"));
}

TEST(RmlItemIconHelpersTest, BuildItemIconDecoratorFallsBackToNoneWhenCatalogUnavailable) {
    EXPECT_EQ(buildItemIconDecorator(nullptr, entt::null), kNoDecorator);
    EXPECT_EQ(buildItemIconDecorator(nullptr, entt::id_type{42}), kNoDecorator);
}

TEST(RmlItemIconHelpersTest, SpriteNameFromIconKeyNormalizesSeparatorsAndCase) {
    EXPECT_EQ(spriteNameFromIconKey("Tools/Watering_Can"), "item-tools-watering-can");
}

} // namespace
} // namespace game::ui
