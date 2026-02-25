#include <gtest/gtest.h>

#include "game/data/appearance_catalog.h"

#include <filesystem>
#include <string>

namespace game::data {
namespace {

[[nodiscard]] std::string catalogPath() {
    return (std::filesystem::path(PROJECT_SOURCE_DIR) / "assets/data/appearance_catalog.json").lexically_normal().string();
}

TEST(AppearanceCatalogTest, ResolveActionSlotVariantToTexturePath) {
    constexpr entt::id_type kNullTexture{};
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(catalogPath()));

    const auto idle_hair = catalog.resolveLayerTexture("idle", "hair", "Standard/Brown", "male");
    ASSERT_TRUE(idle_hair.has_value());
    EXPECT_NE(idle_hair->texture_id_, kNullTexture);
    EXPECT_NE(idle_hair->path_.find("1. Idle/Hair's/Standard/Brown.png"), std::string::npos);
}

TEST(AppearanceCatalogTest, ResolvesGenderSpecificEyesPath) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(catalogPath()));

    const auto male_eyes = catalog.resolveLayerTexture("walk", "eyes", "Blue", "male");
    const auto female_eyes = catalog.resolveLayerTexture("walk", "eyes", "Blue", "female");
    ASSERT_TRUE(male_eyes.has_value());
    ASSERT_TRUE(female_eyes.has_value());
    EXPECT_NE(male_eyes->path_.find("/Eyes/Male/Blue.png"), std::string::npos);
    EXPECT_NE(female_eyes->path_.find("/Eyes/Female/Blue.png"), std::string::npos);
}

TEST(AppearanceCatalogTest, BuildsActionAvailableSlotsMatrix) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(catalogPath()));

    EXPECT_TRUE(catalog.isSlotAvailableForAction("idle", "hair"));
    EXPECT_FALSE(catalog.isSlotAvailableForAction("idle", "weapon"));
    EXPECT_TRUE(catalog.isSlotAvailableForAction("hoe", "weapon"));
}

TEST(AppearanceCatalogTest, RejectsUnknownActionOrVariant) {
    AppearanceCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(catalogPath()));

    EXPECT_FALSE(catalog.resolveLayerTexture("unknown_action", "hair", "Standard/Brown", "male").has_value());
    EXPECT_FALSE(catalog.resolveLayerTexture("idle", "hair", "NotExists/Variant", "male").has_value());
}

} // namespace
} // namespace game::data
