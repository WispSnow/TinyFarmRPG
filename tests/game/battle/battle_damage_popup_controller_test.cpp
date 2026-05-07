#include <gtest/gtest.h>

#include "game/scene/battle_damage_popup_controller.h"

#include <optional>
#include <vector>

namespace game::scene {
namespace {

[[nodiscard]] std::vector<BattlePresentationUnitAnchor> makeAnchors() {
    return {
        BattlePresentationUnitAnchor{
            .unit_id = 1,
            .side = game::battle::BattleSide::Player,
            .base_screen_position = glm::vec2{480.0f, 172.0f},
            .alive_after = true},
        BattlePresentationUnitAnchor{
            .unit_id = 2,
            .side = game::battle::BattleSide::Enemy,
            .base_screen_position = glm::vec2{160.0f, 172.0f},
            .alive_after = true}
    };
}

[[nodiscard]] game::battle::BattleActionResult makeResult(
    const std::optional<game::battle::BattleUnitId> target_id = game::battle::BattleUnitId{2}) {
    game::battle::BattleActionResult result{};
    result.status = game::battle::BattleActionStatus::Applied;
    result.action_type = game::battle::BattleActionType::Attack;
    result.actor_id = 1;
    result.target_id = target_id;
    result.damage = 24;
    return result;
}

TEST(BattleDamagePopupControllerTest, DamagePopupUsesTargetAnchorAndImpactDelay) {
    BattleDamagePopupController controller;
    controller.spawnFromResult(makeResult(), makeAnchors());

    ASSERT_EQ(controller.activePopups().size(), 1U);
    const auto& popup = controller.activePopups().front();
    EXPECT_EQ(popup.text, "24");
    EXPECT_EQ(popup.kind, BattleDamagePopupKind::HpDamage);
    EXPECT_EQ(popup.base_screen_position, glm::vec2(160.0f, 120.0f));
    EXPECT_NEAR(popup.delay_seconds, 0.22f, 0.001f);
    EXPECT_FALSE(popup.visible);
    EXPECT_FLOAT_EQ(popup.alpha, 0.0f);
}

TEST(BattleDamagePopupControllerTest, DelayHoldsPopupBeforeRiseAndFadeAnimationStarts) {
    BattleDamagePopupController controller;
    controller.spawnFromResult(makeResult(), makeAnchors());

    controller.update(0.10f);
    ASSERT_EQ(controller.activePopups().size(), 1U);
    EXPECT_FALSE(controller.activePopups().front().visible);
    EXPECT_EQ(controller.activePopups().front().offset, glm::vec2(0.0f));

    controller.update(0.13f);
    ASSERT_EQ(controller.activePopups().size(), 1U);
    const auto& popup = controller.activePopups().front();
    EXPECT_TRUE(popup.visible);
    EXPECT_GT(popup.alpha, 0.0f);
    EXPECT_LT(popup.offset.y, 0.0f);
}

TEST(BattleDamagePopupControllerTest, RecoveryAndMpPopupsUseSignedTextAndKinds) {
    auto result = makeResult();
    result.damage = 0;
    result.hp_recovered = 18;
    result.mp_recovered = 5;

    BattleDamagePopupController controller;
    controller.spawnFromResult(result, makeAnchors());

    ASSERT_EQ(controller.activePopups().size(), 2U);
    EXPECT_EQ(controller.activePopups()[0].text, "+18");
    EXPECT_EQ(controller.activePopups()[0].kind, BattleDamagePopupKind::HpRecover);
    EXPECT_EQ(controller.activePopups()[1].text, "+5");
    EXPECT_EQ(controller.activePopups()[1].kind, BattleDamagePopupKind::MpRecover);
    EXPECT_FLOAT_EQ(controller.activePopups()[1].base_screen_position.x - controller.activePopups()[0].base_screen_position.x,
                    -18.0f);
}

TEST(BattleDamagePopupControllerTest, MissPopupSkipsDamageText) {
    auto result = makeResult();
    result.missed = true;
    result.damage = 24;

    BattleDamagePopupController controller;
    controller.spawnFromResult(result, makeAnchors());

    ASSERT_EQ(controller.activePopups().size(), 1U);
    EXPECT_EQ(controller.activePopups().front().text, "Miss");
    EXPECT_EQ(controller.activePopups().front().kind, BattleDamagePopupKind::Miss);
}

TEST(BattleDamagePopupControllerTest, CriticalAppearsBeforeDamageNumber) {
    auto result = makeResult();
    result.critical = true;

    BattleDamagePopupController controller;
    controller.spawnFromResult(result, makeAnchors());

    ASSERT_EQ(controller.activePopups().size(), 2U);
    EXPECT_EQ(controller.activePopups()[0].text, "Critical!");
    EXPECT_EQ(controller.activePopups()[0].kind, BattleDamagePopupKind::Critical);
    EXPECT_EQ(controller.activePopups()[1].text, "24");
    EXPECT_EQ(controller.activePopups()[1].kind, BattleDamagePopupKind::HpDamage);
    EXPECT_NEAR(controller.activePopups()[1].delay_seconds - controller.activePopups()[0].delay_seconds, 0.12f, 0.001f);
    EXPECT_EQ(controller.activePopups()[1].base_screen_position, controller.activePopups()[0].base_screen_position);
}

TEST(BattleDamagePopupControllerTest, CriticalStaggersAllNumericPopupsAndOffsetsSimultaneousNumbers) {
    auto result = makeResult();
    result.critical = true;
    result.damage = 0;
    result.hp_recovered = 12;
    result.mp_recovered = 3;

    BattleDamagePopupController controller;
    controller.spawnFromResult(result, makeAnchors());

    ASSERT_EQ(controller.activePopups().size(), 3U);
    EXPECT_EQ(controller.activePopups()[0].kind, BattleDamagePopupKind::Critical);
    EXPECT_EQ(controller.activePopups()[1].kind, BattleDamagePopupKind::HpRecover);
    EXPECT_EQ(controller.activePopups()[2].kind, BattleDamagePopupKind::MpRecover);
    EXPECT_NEAR(controller.activePopups()[1].delay_seconds - controller.activePopups()[0].delay_seconds, 0.12f, 0.001f);
    EXPECT_NEAR(controller.activePopups()[2].delay_seconds - controller.activePopups()[0].delay_seconds, 0.12f, 0.001f);
    EXPECT_EQ(controller.activePopups()[1].base_screen_position, controller.activePopups()[0].base_screen_position);
    EXPECT_FLOAT_EQ(controller.activePopups()[2].base_screen_position.x -
                        controller.activePopups()[1].base_screen_position.x,
                    -18.0f);
}

TEST(BattleDamagePopupControllerTest, PopupExpiresAndActiveListBecomesEmpty) {
    BattleDamagePopupController controller;
    controller.spawnFromResult(makeResult(), makeAnchors());

    for (int i = 0; i < 8; ++i) {
        controller.update(0.20f);
    }

    EXPECT_TRUE(controller.activePopups().empty());
}

TEST(BattleDamagePopupControllerTest, MissingTargetFallsBackToActorAnchorForAggregateValue) {
    auto result = makeResult(std::nullopt);

    BattleDamagePopupController controller;
    controller.spawnFromResult(result, makeAnchors());

    // Current BattleActionResult has no per-target payload for aggregate effects, so the actor anchor is a temporary fallback.
    ASSERT_EQ(controller.activePopups().size(), 1U);
    EXPECT_EQ(controller.activePopups().front().text, "24");
    EXPECT_EQ(controller.activePopups().front().base_screen_position, glm::vec2(480.0f, 124.0f));
}

TEST(BattleDamagePopupControllerTest, MissWithoutTargetDoesNotGenerateDamage) {
    auto result = makeResult(std::nullopt);
    result.missed = true;
    result.damage = 24;

    BattleDamagePopupController controller;
    controller.spawnFromResult(result, makeAnchors());

    EXPECT_TRUE(controller.activePopups().empty());
}

TEST(BattleDamagePopupControllerTest, RejectedAndStateOnlyResultsDoNotSpawnPopup) {
    auto rejected = makeResult();
    rejected.status = game::battle::BattleActionStatus::Rejected;

    BattleDamagePopupController controller;
    controller.spawnFromResult(rejected, makeAnchors());
    EXPECT_TRUE(controller.activePopups().empty());

    auto state_only = makeResult();
    state_only.damage = 0;
    state_only.states_added.push_back("state.poison");
    controller.spawnFromResult(state_only, makeAnchors());
    EXPECT_TRUE(controller.activePopups().empty());
}

TEST(BattleDamagePopupControllerTest, ColorMappingAppliesAlpha) {
    const auto damage = battleDamagePopupColor(BattleDamagePopupKind::HpDamage, 0.4f);
    EXPECT_GT(damage.r, damage.g);
    EXPECT_FLOAT_EQ(damage.a, 0.4f);

    const auto heal = battleDamagePopupColor(BattleDamagePopupKind::HpRecover, 2.0f);
    EXPECT_GT(heal.g, heal.r);
    EXPECT_FLOAT_EQ(heal.a, 1.0f);
}

} // namespace
} // namespace game::scene
