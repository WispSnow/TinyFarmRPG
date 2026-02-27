// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/battle/battle_formula_evaluator.h"

#include <string>

namespace game::battle {
namespace {

BattleUnit makeSourceUnit() {
    return BattleUnit{
        .id = 1,
        .name = "Hero",
        .side = BattleSide::Player,
        .hp = 120,
        .max_hp = 140,
        .mp = 30,
        .max_mp = 35,
        .attack = 28,
        .defense = 20,
        .magic_attack = 12,
        .magic_defense = 16,
        .speed = 18,
        .luck = 8};
}

BattleUnit makeTargetUnit() {
    return BattleUnit{
        .id = 1001,
        .name = "Slime",
        .side = BattleSide::Enemy,
        .hp = 60,
        .max_hp = 60,
        .mp = 5,
        .max_mp = 5,
        .attack = 12,
        .defense = 8,
        .magic_attack = 5,
        .magic_defense = 5,
        .speed = 10,
        .luck = 5};
}

TEST(BattleFormulaEvaluatorTest, EvaluatesFormulaWithBattleParams) {
    BattleFormulaEvaluator evaluator{};
    const BattleUnit source = makeSourceUnit();
    const BattleUnit target = makeTargetUnit();

    int value = 0;
    std::string error{};
    const bool ok = evaluator.evaluate("a.atk * 4 - b.def * 2", source, target, value, error);

#ifdef TF_ENABLE_SCRIPTING
    ASSERT_TRUE(ok) << error;
    EXPECT_EQ(value, 96);
#else
    EXPECT_FALSE(ok);
    EXPECT_NE(error.find("disabled"), std::string::npos);
#endif
}

TEST(BattleFormulaEvaluatorTest, SupportsMathHelpersAndMhpMmpFields) {
    BattleFormulaEvaluator evaluator{};
    const BattleUnit source = makeSourceUnit();
    const BattleUnit target = makeTargetUnit();

    int value = 0;
    std::string error{};
    const bool ok = evaluator.evaluate("math.max(0, a.mhp - b.mhp) + (a.mmp - b.mmp)", source, target, value, error);

#ifdef TF_ENABLE_SCRIPTING
    ASSERT_TRUE(ok) << error;
    EXPECT_EQ(value, 110);
#else
    EXPECT_FALSE(ok);
#endif
}

TEST(BattleFormulaEvaluatorTest, RejectsInvalidFormulaSyntax) {
    BattleFormulaEvaluator evaluator{};
    const BattleUnit source = makeSourceUnit();
    const BattleUnit target = makeTargetUnit();

    int value = 0;
    std::string error{};
    const bool ok = evaluator.evaluate("a.atk *** b.def", source, target, value, error);

    EXPECT_FALSE(ok);
    EXPECT_FALSE(error.empty());
}

TEST(BattleFormulaEvaluatorTest, RejectsNonNumericResult) {
    BattleFormulaEvaluator evaluator{};
    const BattleUnit source = makeSourceUnit();
    const BattleUnit target = makeTargetUnit();

    int value = 0;
    std::string error{};
    const bool ok = evaluator.evaluate("'not_number'", source, target, value, error);

    EXPECT_FALSE(ok);
    EXPECT_FALSE(error.empty());
}

TEST(BattleFormulaEvaluatorTest, SupportsDamageFormulaDataOverload) {
    BattleFormulaEvaluator evaluator{};
    const BattleUnit source = makeSourceUnit();
    const BattleUnit target = makeTargetUnit();

    game::data::DamageFormulaData damage{};
    damage.formula = "a.mat * 3 - b.mdf";

    int value = 0;
    std::string error{};
    const bool ok = evaluator.evaluate(damage, source, target, value, error);

#ifdef TF_ENABLE_SCRIPTING
    ASSERT_TRUE(ok) << error;
    EXPECT_EQ(value, 31);
#else
    EXPECT_FALSE(ok);
#endif
}

} // namespace
} // namespace game::battle
// NOLINTEND
