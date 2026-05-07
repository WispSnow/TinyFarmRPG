// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::size_t countOccurrences(const std::string& source, const std::string& needle) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = source.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

[[nodiscard]] std::string snippetFrom(const std::string& source, const std::string& anchor, const std::size_t length = 320U) {
    const auto position = source.find(anchor);
    if (position == std::string::npos) {
        return {};
    }
    return source.substr(position, length);
}

} // namespace

namespace game::scene {
namespace {

TEST(BattleSceneSmokeTest, ContainsStateMachineStages) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("FlowState::WaitingForInput"), std::string::npos);
    EXPECT_NE(source.find("FlowState::ExecutingAction"), std::string::npos);
    EXPECT_NE(source.find("FlowState::AnimatingResult"), std::string::npos);
    EXPECT_NE(source.find("FlowState::CheckVictory"), std::string::npos);
    EXPECT_NE(source.find("FlowState::NextTurn"), std::string::npos);
    EXPECT_NE(source.find("FlowState::BattleEnd"), std::string::npos);
}

TEST(BattleSceneSmokeTest, EmitsBattleEndedEventAndRequestsPop) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("BattleEndedEvent"), std::string::npos);
    EXPECT_NE(source.find("requestPopScene()"), std::string::npos);
}

TEST(BattleSceneSmokeTest, UsesTypedModelAndSceneLevelMenuInput) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("createModel(MODEL_NAME, &type_register_)"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<MainActionViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<ListEntryViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<TargetEntryViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<PartyStatusViewModel>"), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"party_status\""), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_up\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_down\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_left\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_right\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_confirm\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_cancel\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("Focus(true)"), std::string::npos);
    EXPECT_NE(source.find("constexpr int MAIN_ACTION_COLUMNS = 2;"), std::string::npos);
    EXPECT_NE(source.find("rpg_catalog_(session_options.rpg_catalog)"), std::string::npos);
    EXPECT_NE(source.find("item_catalog_(session_options.item_catalog)"), std::string::npos);
    EXPECT_NE(source.find("populateSkillEntries"), std::string::npos);
    EXPECT_NE(source.find("setMenuState(MenuState::SkillList)"), std::string::npos);
    EXPECT_NE(source.find("populateItemEntries"), std::string::npos);
    EXPECT_NE(source.find("setMenuState(MenuState::ItemList)"), std::string::npos);
    EXPECT_NE(source.find("setMenuState(MenuState::MainMenu)"), std::string::npos);
    EXPECT_EQ(source.find("units_text"), std::string::npos);
}

TEST(BattleSceneSmokeTest, WiresStage2SkillListToDraftSelection) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("actor.skill_ids"), std::string::npos);
    EXPECT_NE(source.find("findSkill(skill_id)"), std::string::npos);
    EXPECT_NE(source.find("isSkillEntryEnabled"), std::string::npos);
    EXPECT_NE(source.find("selected_skill_id = skill->id_"), std::string::npos);
    EXPECT_NE(source.find("continueDraftAfterScopeSelected(skill->scope_, *actor)"), std::string::npos);
    EXPECT_NE(source.find("menuStateForActionDraftSource"), std::string::npos);
    EXPECT_EQ(source.find("enterListMenu(MenuState::SkillList)"), std::string::npos);
}

TEST(BattleSceneSmokeTest, WiresStage3ItemListToDraftSelection) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("session_.itemStocks()"), std::string::npos);
    EXPECT_NE(source.find("item_catalog_->listItems()"), std::string::npos);
    EXPECT_NE(source.find("findBattleItemByEntryId"), std::string::npos);
    EXPECT_NE(source.find("selected_item_id = item->id_str_"), std::string::npos);
    EXPECT_NE(source.find("continueDraftAfterScopeSelected(item->battle_use_->scope, *actor)"), std::string::npos);
    EXPECT_NE(source.find("remaining_item_stocks = session_.itemStocks()"), std::string::npos);
    EXPECT_EQ(source.find("enterListMenu(MenuState::ItemList)"), std::string::npos);
}

TEST(BattleSceneSmokeTest, WiresStage4TargetSelectionAndDraftSubmit) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("populateTargetEntries"), std::string::npos);
    EXPECT_NE(source.find("findTargetEntry"), std::string::npos);
    EXPECT_NE(source.find("firstEnabledTargetEntryIndex"), std::string::npos);
    EXPECT_NE(source.find("continueDraftAfterScopeSelected"), std::string::npos);
    EXPECT_NE(source.find("submitDraftAction"), std::string::npos);
    EXPECT_NE(source.find("selected_target_id = static_cast<game::battle::BattleUnitId>(entry->unit_id)"), std::string::npos);
    EXPECT_NE(source.find("continueDraftAfterScopeSelected(game::data::Scope::OneEnemy, *actor)"), std::string::npos);
    EXPECT_NE(source.find("case game::data::Scope::Self:"), std::string::npos);
    EXPECT_NE(source.find("case game::data::Scope::AllEnemies:"), std::string::npos);
    EXPECT_NE(source.find("case game::data::Scope::AllAllies:"), std::string::npos);
    EXPECT_NE(source.find("setMenuState(menuStateForActionDraftSource())"), std::string::npos);
    EXPECT_EQ(source.find("selectDefaultTarget("), std::string::npos);
    EXPECT_EQ(source.find("Target selection coming in Stage 4"), std::string::npos);
    EXPECT_EQ(source.find("enterTargetPlaceholder"), std::string::npos);
}

TEST(BattleSceneSmokeTest, RoutesEnemyTurnsWithoutFallingBackToPlayerMenu) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("BattleAiPlanner::planEnemyAction"), std::string::npos);
    EXPECT_NE(source.find("BattleAiPlanner::planFallbackAction"), std::string::npos);
    EXPECT_NE(source.find("actor->side == game::battle::BattleSide::Enemy"), std::string::npos);

    const std::string next_turn_block = snippetFrom(source, "case FlowState::NextTurn:");
    ASSERT_FALSE(next_turn_block.empty());
    EXPECT_EQ(next_turn_block.find("state_ = FlowState::WaitingForInput"), std::string::npos);
    EXPECT_EQ(next_turn_block.find("enterInputMenu()"), std::string::npos);

    const std::string missing_pending_block = snippetFrom(source, "if (!pending_action_) {");
    ASSERT_FALSE(missing_pending_block.empty());
    EXPECT_EQ(missing_pending_block.find("state_ = FlowState::WaitingForInput"), std::string::npos);
    EXPECT_EQ(missing_pending_block.find("enterInputMenu()"), std::string::npos);
}

TEST(BattleSceneSmokeTest, FormatsStage5RecoveryResultText) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("formatActionResultText"), std::string::npos);
    EXPECT_NE(source.find("formatRecoveryText"), std::string::npos);
    EXPECT_NE(source.find("result.hp_recovered > 0"), std::string::npos);
    EXPECT_NE(source.find("result.mp_recovered > 0"), std::string::npos);
    EXPECT_NE(source.find("result.damage > 0"), std::string::npos);
    EXPECT_NE(source.find("Result: Item used"), std::string::npos);
    EXPECT_EQ(source.find("dealt 0 dmg"), std::string::npos);
}

TEST(BattleSceneSmokeTest, RendersSideViewSpritesOverOpaqueBattlefield) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = readTextFile(header_path);
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());

    EXPECT_NE(header.find("void render(float interpolation_alpha) override"), std::string::npos);
    EXPECT_NE(source.find("renderBattlefieldBackground"), std::string::npos);
    EXPECT_NE(source.find("drawFilledRect"), std::string::npos);
    EXPECT_NE(source.find("context_.getRenderer().beginFrame(context_.getCamera())"), std::string::npos);
    EXPECT_NE(source.find("BattleSpriteComponent"), std::string::npos);
    EXPECT_NE(source.find("battle_render_system_.renderPrepared"), std::string::npos);
    EXPECT_NE(source.find("battleFormationSlot"), std::string::npos);
    EXPECT_NE(source.find("constexpr float BATTLEFIELD_HEIGHT = 256.0f;"), std::string::npos);
    EXPECT_NE(source.find("BATTLE_SPRITE_SCALE_MULTIPLIER"), std::string::npos);
    EXPECT_NE(source.find("glm::vec2{480.0F, 172.0F}"), std::string::npos);
    EXPECT_NE(source.find("glm::vec2{160.0F, 172.0F}"), std::string::npos);
    EXPECT_NE(source.find("std::clamp(position.y, 96.0F, BATTLEFIELD_HEIGHT - 30.0F)"), std::string::npos);
    EXPECT_NE(source.find("glm::vec2{18.0F, 28.0F}"), std::string::npos);
    EXPECT_NE(source.find("glm::vec2{-18.0F, 30.0F}"), std::string::npos);
    EXPECT_NE(source.find("shadow_size"), std::string::npos);
    EXPECT_NE(source.find("idle_left"), std::string::npos);
    EXPECT_NE(source.find("enemy->battle_visual_"), std::string::npos);
    EXPECT_NE(source.find("AppearanceLayerCacheBuilder::rebuild"), std::string::npos);
    EXPECT_EQ(source.find("AppearanceSystem"), std::string::npos);
    EXPECT_EQ(source.find("unit.side == game::battle::BattleSide::Player ? 454.0F : 186.0F"), std::string::npos);
}

TEST(BattleSceneSmokeTest, BattleBackgroundDrawsGroundThenAlphaBackdrop) {
    const std::filesystem::path background_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_background.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(background_source_path)) << background_source_path;

    const std::string background_source = readTextFile(background_source_path);
    ASSERT_FALSE(background_source.empty());

    const std::string render_snippet = snippetFrom(background_source, "void BattleBackgroundRenderer::render", 1400U);
    ASSERT_FALSE(render_snippet.empty());
    const std::size_t ground_pos = render_snippet.find("if (ground_.valid)");
    const std::size_t backdrop_pos = render_snippet.find("if (backdrop_.valid)");
    ASSERT_NE(ground_pos, std::string::npos);
    ASSERT_NE(backdrop_pos, std::string::npos);
    EXPECT_LT(ground_pos, backdrop_pos);
    EXPECT_NE(render_snippet.find("const engine::utils::Rect screen_rect{glm::vec2{0.0f, 0.0f}, logical_size}"),
              std::string::npos);
    EXPECT_NE(render_snippet.find("computeBottomAnchoredCropDrawRect(ground_.texture_size, screen_rect)"),
              std::string::npos);
    EXPECT_NE(render_snippet.find("computeTopAnchoredCropDrawRect(backdrop_.texture_size, screen_rect)"),
              std::string::npos);
    EXPECT_EQ(render_snippet.find("BATTLEFIELD_HEIGHT"), std::string::npos);

    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;

    const std::string scene_source = readTextFile(scene_source_path);
    const std::string battlefield_snippet = snippetFrom(scene_source, "void BattleScene::renderBattlefieldBackground", 1200U);
    ASSERT_FALSE(battlefield_snippet.empty());
    EXPECT_NE(battlefield_snippet.find("battle_background_.render(renderer, camera)"), std::string::npos);
    EXPECT_EQ(battlefield_snippet.find("BATTLEFIELD_HEIGHT - 4.0F"), std::string::npos);
}

TEST(BattleSceneSmokeTest, RmlUsesDataDrivenBattleMenuBindings) {
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/battle.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string rml = readTextFile(rml_path);
    ASSERT_FALSE(rml.empty());

    EXPECT_NE(rml.find("../theme/nav.rcss"), std::string::npos);
    EXPECT_NE(rml.find("../theme/portrait.rcss"), std::string::npos);
    EXPECT_NE(rml.find("tf-screen-root tf-nav-root"), std::string::npos);
    EXPECT_EQ(rml.find("Battle Prototype"), std::string::npos);
    EXPECT_EQ(rml.find("tf-button-secondary"), std::string::npos);
    EXPECT_EQ(rml.find("tf-button-primary"), std::string::npos);
    EXPECT_EQ(rml.find("<progress"), std::string::npos);
    EXPECT_GE(countOccurrences(rml, "tf-nav-auto"), 3U);
    EXPECT_NE(rml.find("id=\"battle-top-status\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-turn\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-result\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"member : party_status\""), std::string::npos);
    EXPECT_NE(rml.find("battle-party-identity"), std::string::npos);
    EXPECT_NE(rml.find("data-style-width=\"member.hp_ratio_percent\""), std::string::npos);
    EXPECT_NE(rml.find("data-style-width=\"member.mp_ratio_percent\""), std::string::npos);
    EXPECT_NE(rml.find("battle-party-main"), std::string::npos);
    EXPECT_NE(rml.find("data-style-decorator=\"member.portrait_decorator\""), std::string::npos);
    EXPECT_EQ(rml.find("data-class-portrait-player"), std::string::npos);
    EXPECT_EQ(rml.find("member.portrait_player"), std::string::npos);
    EXPECT_EQ(rml.find("member.portrait_lyria"), std::string::npos);
    EXPECT_EQ(rml.find("member.portrait_tori"), std::string::npos);
    EXPECT_EQ(rml.find("battle-menu-title"), std::string::npos);
    EXPECT_EQ(rml.find("battle-menu-hint"), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"action : main_actions\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"main_action_select(action.entry_index)\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"list_menu_visible\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"entry : list_entries\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"target_menu_visible\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"target : target_entries\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"target_entry_select(target.entry_index)\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-is-ally=\"target.is_ally\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-is-dead=\"target.is_dead\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-disabled=\"!target.enabled\""), std::string::npos);
}

TEST(BattleSceneSmokeTest, RcssDefinesStage5BattleMenuStates) {
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/battle.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;

    const std::string rcss = readTextFile(rcss_path);
    ASSERT_FALSE(rcss.empty());

    EXPECT_NE(rcss.find("top: 256dp;"), std::string::npos);
    EXPECT_NE(rcss.find("height: 104dp;"), std::string::npos);
    EXPECT_NE(rcss.find("left: 464dp;"), std::string::npos);
    EXPECT_NE(rcss.find("width: 168dp;"), std::string::npos);
    EXPECT_EQ(rcss.find("width: 302dp"), std::string::npos);
    EXPECT_EQ(rcss.find("#battle-menu-title"), std::string::npos);
    EXPECT_EQ(rcss.find("#battle-menu-hint"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-list-entry.disabled"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-text-button"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-hp-fill"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-mp-fill"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-hud"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-list-entry.disabled .battle-entry-sublabel"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-target-entry.is-ally"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-target-entry.is-dead"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-target-entry.disabled"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-list-empty"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-target-empty"), std::string::npos);
    EXPECT_EQ(rcss.find("solid"), std::string::npos);
    EXPECT_EQ(rcss.find("font-style: italic"), std::string::npos);
    EXPECT_EQ(rcss.find("ninepatch"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
