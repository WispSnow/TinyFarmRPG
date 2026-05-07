#include <gtest/gtest.h>

#include "game/scene/battle_background.h"

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace game::scene {
namespace {

TEST(BattleBackgroundTest, ValidatesSimpleIdsOnly) {
    EXPECT_TRUE(isValidBattleBackgroundId("Grassland"));
    EXPECT_TRUE(isValidBattleBackgroundId("Cave_01"));

    EXPECT_FALSE(isValidBattleBackgroundId(""));
    EXPECT_FALSE(isValidBattleBackgroundId("Grassland.png"));
    EXPECT_FALSE(isValidBattleBackgroundId("../Grassland"));
    EXPECT_FALSE(isValidBattleBackgroundId("battlebacks1/Grassland"));
    EXPECT_FALSE(isValidBattleBackgroundId("Grassland-01"));
}

TEST(BattleBackgroundTest, BuildsDistinctLayerRefs) {
    const auto ground = makeBattleBackgroundLayerRef("Grassland", BattleBackgroundLayerKind::Ground);
    const auto backdrop = makeBattleBackgroundLayerRef("Grassland", BattleBackgroundLayerKind::Backdrop);
    constexpr entt::id_type null_texture_id{entt::null};

    EXPECT_NE(ground.texture_id, null_texture_id);
    EXPECT_NE(backdrop.texture_id, null_texture_id);
    EXPECT_NE(ground.texture_id, backdrop.texture_id);
    EXPECT_EQ(ground.path, "assets/textures/BattleBg/battlebacks1/Grassland.png");
    EXPECT_EQ(backdrop.path, "assets/textures/BattleBg/battlebacks2/Grassland.png");
}

TEST(BattleBackgroundTest, InvalidIdBuildsEmptyLayerRef) {
    const auto ref = makeBattleBackgroundLayerRef("../Grassland", BattleBackgroundLayerKind::Ground);
    constexpr entt::id_type null_texture_id{entt::null};

    EXPECT_EQ(ref.texture_id, null_texture_id);
    EXPECT_TRUE(ref.path.empty());
}

TEST(BattleBackgroundTest, BackdropCropSamplesRpgMakerImageTopWithoutStretch) {
    const engine::utils::Rect screen{glm::vec2{0.0f, 0.0f}, glm::vec2{640.0f, 360.0f}};
    const auto result = computeTopAnchoredCropDrawRect(glm::vec2{1000.0f, 740.0f}, screen);

    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(result->screen_rect.pos.x, 0.0f);
    EXPECT_FLOAT_EQ(result->screen_rect.pos.y, 0.0f);
    EXPECT_FLOAT_EQ(result->screen_rect.size.x, 640.0f);
    EXPECT_FLOAT_EQ(result->screen_rect.size.y, 360.0f);
    EXPECT_FLOAT_EQ(result->source_rect.pos.x, 0.0f);
    EXPECT_FLOAT_EQ(result->source_rect.size.x, 1000.0f);
    EXPECT_FLOAT_EQ(result->source_rect.size.y, 562.5f);
    EXPECT_FLOAT_EQ(result->source_rect.pos.y, 31.95f);
    EXPECT_FLOAT_EQ(result->source_rect.size.x / result->source_rect.size.y,
                    result->screen_rect.size.x / result->screen_rect.size.y);
}

TEST(BattleBackgroundTest, GroundCropSamplesRpgMakerImageBottomWithoutStretch) {
    const engine::utils::Rect screen{glm::vec2{0.0f, 0.0f}, glm::vec2{640.0f, 360.0f}};
    const auto result = computeBottomAnchoredCropDrawRect(glm::vec2{1000.0f, 740.0f}, screen);

    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(result->screen_rect.pos.x, 0.0f);
    EXPECT_FLOAT_EQ(result->screen_rect.pos.y, 0.0f);
    EXPECT_FLOAT_EQ(result->screen_rect.size.x, 640.0f);
    EXPECT_FLOAT_EQ(result->screen_rect.size.y, 360.0f);
    EXPECT_FLOAT_EQ(result->source_rect.pos.x, 0.0f);
    EXPECT_FLOAT_EQ(result->source_rect.pos.y, 177.5f);
    EXPECT_FLOAT_EQ(result->source_rect.size.x, 1000.0f);
    EXPECT_FLOAT_EQ(result->source_rect.size.y, 562.5f);
    EXPECT_NEAR(result->source_rect.pos.y + result->source_rect.size.y, 740.0f, 0.001f);
    EXPECT_FLOAT_EQ(result->source_rect.size.x / result->source_rect.size.y,
                    result->screen_rect.size.x / result->screen_rect.size.y);
}

} // namespace
} // namespace game::scene
