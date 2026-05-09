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
    EXPECT_NE(source.find("FlowState::VictoryFlow"), std::string::npos);
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
    EXPECT_NE(source.find("event.reward_summary"), std::string::npos);
}

TEST(BattleSceneSmokeTest, VictoryFlowDelaysBattleEndedEventUntilConfirm) {
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

    EXPECT_NE(header.find("#include \"game/scene/battle_victory_flow_controller.h\""), std::string::npos);
    EXPECT_NE(header.find("BattleVictoryFlowController victory_flow_controller_{}"), std::string::npos);
    EXPECT_NE(header.find("std::optional<game::battle::BattleRewardSummary> victory_reward_summary_{}"), std::string::npos);
    EXPECT_NE(header.find("beginVictoryFlow"), std::string::npos);
    EXPECT_NE(header.find("finishVictoryFlow"), std::string::npos);

    const std::string check_block = snippetFrom(source, "case FlowState::CheckVictory:", 900U);
    ASSERT_FALSE(check_block.empty());
    EXPECT_NE(check_block.find("beginVictoryFlow();"), std::string::npos);
    EXPECT_NE(check_block.find("state_ = FlowState::BattleEnd"), std::string::npos);
    EXPECT_LT(check_block.find("beginVictoryFlow();"), check_block.find("state_ = FlowState::BattleEnd"));

    const std::string victory_block = snippetFrom(source, "case FlowState::VictoryFlow:", 360U);
    ASSERT_FALSE(victory_block.empty());
    EXPECT_NE(victory_block.find("victory_flow_controller_.update(delta_time)"), std::string::npos);
    EXPECT_NE(victory_block.find("finishVictoryFlow();"), std::string::npos);
    EXPECT_EQ(victory_block.find("requestBattleEnd()"), std::string::npos);

    const std::string confirm_block = snippetFrom(source, "bool BattleScene::onMenuConfirmPressed()", 600U);
    ASSERT_FALSE(confirm_block.empty());
    EXPECT_NE(confirm_block.find("state_ == FlowState::VictoryFlow"), std::string::npos);
    EXPECT_NE(confirm_block.find("victory_flow_controller_.confirm();"), std::string::npos);

    const std::string cancel_block = snippetFrom(source, "bool BattleScene::onMenuCancelPressed()", 260U);
    ASSERT_FALSE(cancel_block.empty());
    EXPECT_NE(cancel_block.find("state_ == FlowState::VictoryFlow"), std::string::npos);
    EXPECT_EQ(cancel_block.find("victory_flow_controller_.confirm();"), std::string::npos);

    const std::string finish_block = snippetFrom(source, "void BattleScene::finishVictoryFlow()", 260U);
    ASSERT_FALSE(finish_block.empty());
    EXPECT_NE(finish_block.find("victory_flow_controller_.reset();"), std::string::npos);
    EXPECT_NE(finish_block.find("state_ = FlowState::BattleEnd;"), std::string::npos);
    EXPECT_EQ(finish_block.find("requestBattleEnd()"), std::string::npos);
}

TEST(BattleSceneSmokeTest, UsesTypedModelAndSceneLevelMenuInput) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("createModel(MODEL_NAME, &type_register_)"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<CommandViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<ListEntryViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<TargetEntryViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<PartyStatusViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<StateIconViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<StateTooltipViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<BattleLogEntryViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterMember(\"text\", &BattleLogEntryViewModel::text)"), std::string::npos);
    EXPECT_NE(source.find("RegisterMember(\"tone_class\", &BattleLogEntryViewModel::tone_class)"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<VictoryRewardItemViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterStruct<TurnOrderEntryViewModel>"), std::string::npos);
    EXPECT_NE(source.find("RegisterMember(\"badge_label\""), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"party_status\""), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"party_state_icons\""), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"state_tooltip\""), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"battle_log_entries\""), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"victory_overlay_visible\""), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"victory_reward_items\""), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"turn_order_entries\""), std::string::npos);
    EXPECT_NE(source.find("state_icon_hover_enter"), std::string::npos);
    EXPECT_NE(source.find("state_icon_hover_exit"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_up\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_down\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_left\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_right\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_confirm\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("onAction(\"menu_cancel\"_hs)"), std::string::npos);
    EXPECT_NE(source.find("Focus(true)"), std::string::npos);
    EXPECT_NE(source.find("focusElementById(\"battle-victory-continue\")"), std::string::npos);
    EXPECT_NE(source.find("constexpr int PARTY_COMMAND_COLUMNS = 1;"), std::string::npos);
    EXPECT_NE(source.find("constexpr int ACTOR_COMMAND_COLUMNS = 2;"), std::string::npos);
    EXPECT_NE(source.find("rpg_catalog_(session_options.rpg_catalog)"), std::string::npos);
    EXPECT_NE(source.find("item_catalog_(session_options.item_catalog)"), std::string::npos);
    EXPECT_NE(source.find("populateSkillEntries"), std::string::npos);
    EXPECT_NE(source.find("setMenuState(MenuState::SkillList)"), std::string::npos);
    EXPECT_NE(source.find("populateItemEntries"), std::string::npos);
    EXPECT_NE(source.find("setMenuState(MenuState::ItemList)"), std::string::npos);
    EXPECT_NE(source.find("setMenuState(MenuState::PartyCommand)"), std::string::npos);
    EXPECT_NE(source.find("setMenuState(MenuState::ActorCommand)"), std::string::npos);
    EXPECT_NE(source.find("enemyTurnOrderIconDecorator"), std::string::npos);
    EXPECT_NE(source.find("battleEnemyIconSpriteName"), std::string::npos);
    EXPECT_NE(source.find("findEnemyIdleDownAnimation"), std::string::npos);
    EXPECT_NE(source.find("\"idle_down\"_hs"), std::string::npos);
    EXPECT_NE(source.find("sprite_blueprint_id_hash_"), std::string::npos);
    EXPECT_NE(source.find("turnOrderFallbackLabel(unit.side, side_index)"), std::string::npos);
    EXPECT_EQ(source.find("units_text"), std::string::npos);
    EXPECT_EQ(source.find("RegisterStruct<MainActionViewModel>"), std::string::npos);
    EXPECT_EQ(countOccurrences(source, "RegisterArray<decltype(party_commands_)>()"), 1U);
    EXPECT_EQ(source.find("RegisterArray<decltype(actor_commands_)>()"), std::string::npos);
    EXPECT_NE(source.find("RegisterArray<decltype(battle_log_entries_)>()"), std::string::npos);
    EXPECT_NE(source.find("RegisterArray<decltype(victory_reward_items_)>()"), std::string::npos);
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

TEST(BattleSceneSmokeTest, SkillListFiltersBasicAttackCommandSkill) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("constexpr std::string_view BASIC_ATTACK_SKILL_ID = \"skill.attack\""), std::string::npos);
    EXPECT_NE(source.find("bool isActorSkillMenuEntry(std::string_view skill_id)"), std::string::npos);

    const std::string skill_list_block = snippetFrom(source, "void BattleScene::populateSkillEntries", 1200U);
    ASSERT_FALSE(skill_list_block.empty());
    EXPECT_NE(skill_list_block.find("if (!isActorSkillMenuEntry(skill_id))"), std::string::npos);
    EXPECT_LT(skill_list_block.find("if (!isActorSkillMenuEntry(skill_id))"),
              skill_list_block.find("findSkill(skill_id)"));
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

TEST(BattleSceneSmokeTest, WiresRpgMakerStylePartyAndActorCommands) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    const std::filesystem::path session_header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/battle/battle_session.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(session_header_path)) << session_header_path;

    const std::string header = readTextFile(header_path);
    const std::string source = readTextFile(source_path);
    const std::string session_header = readTextFile(session_header_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(session_header.empty());

    EXPECT_NE(header.find("PartyCommand"), std::string::npos);
    EXPECT_NE(header.find("ActorCommand"), std::string::npos);
    EXPECT_NE(header.find("party_command_accepted_round_"), std::string::npos);
    EXPECT_NE(header.find("actor_command_entered_via_fight_this_step_"), std::string::npos);
    EXPECT_NE(session_header.find("roundIndex() const"), std::string::npos);
    EXPECT_NE(source.find("enum class PartyCommandId"), std::string::npos);
    EXPECT_NE(source.find("enum class ActorCommandId"), std::string::npos);
    EXPECT_NE(source.find("populatePartyCommands"), std::string::npos);
    EXPECT_NE(source.find("populateActorCommands"), std::string::npos);
    EXPECT_NE(source.find("shouldOpenPartyCommand"), std::string::npos);
    EXPECT_NE(source.find("party_command_accepted_round_ != session_.roundIndex()"), std::string::npos);

    const std::string party_block = snippetFrom(source, "void BattleScene::populatePartyCommands()", 800U);
    ASSERT_FALSE(party_block.empty());
    EXPECT_NE(party_block.find("PartyCommandId::Fight"), std::string::npos);
    EXPECT_NE(party_block.find("PartyCommandId::Escape"), std::string::npos);

    const std::string actor_block = snippetFrom(source, "void BattleScene::populateActorCommands()", 900U);
    ASSERT_FALSE(actor_block.empty());
    EXPECT_NE(actor_block.find("ActorCommandId::Attack"), std::string::npos);
    EXPECT_NE(actor_block.find("ActorCommandId::Skill"), std::string::npos);
    EXPECT_NE(actor_block.find("ActorCommandId::Guard"), std::string::npos);
    EXPECT_NE(actor_block.find("ActorCommandId::Item"), std::string::npos);
    EXPECT_EQ(actor_block.find("Escape"), std::string::npos);
    EXPECT_EQ(actor_block.find("EndTurn"), std::string::npos);

    const std::string fight_case = snippetFrom(source, "case PartyCommandId::Fight:");
    ASSERT_FALSE(fight_case.empty());
    EXPECT_NE(fight_case.find("party_command_accepted_round_ = session_.roundIndex();"), std::string::npos);
    EXPECT_NE(fight_case.find("actor_command_entered_via_fight_this_step_ = true;"), std::string::npos);
    EXPECT_NE(fight_case.find("setMenuState(MenuState::ActorCommand)"), std::string::npos);

    const std::string escape_case = snippetFrom(source, "case PartyCommandId::Escape:");
    ASSERT_FALSE(escape_case.empty());
    EXPECT_NE(escape_case.find("queueEscapeAction();"), std::string::npos);
    EXPECT_EQ(escape_case.find("party_command_accepted_round_ ="), std::string::npos);

    const std::string cancel_block = snippetFrom(source, "bool BattleScene::onMenuCancelPressed()", 1400U);
    ASSERT_FALSE(cancel_block.empty());
    EXPECT_NE(cancel_block.find("case MenuState::SkillList:"), std::string::npos);
    EXPECT_NE(cancel_block.find("case MenuState::ItemList:"), std::string::npos);
    EXPECT_NE(cancel_block.find("actor_command_entered_via_fight_this_step_ = false;"), std::string::npos);
    EXPECT_NE(cancel_block.find("setMenuState(MenuState::ActorCommand)"), std::string::npos);
    EXPECT_NE(cancel_block.find("case MenuState::ActorCommand:"), std::string::npos);
    EXPECT_NE(cancel_block.find("if (actor_command_entered_via_fight_this_step_)"), std::string::npos);
    EXPECT_NE(cancel_block.find("party_command_accepted_round_.reset();"), std::string::npos);
    EXPECT_NE(cancel_block.find("setMenuState(MenuState::PartyCommand)"), std::string::npos);
    EXPECT_EQ(source.find("MenuState::MainMenu"), std::string::npos);
    EXPECT_EQ(source.find("queueEndTurnAction"), std::string::npos);

    EXPECT_EQ(source.find("back_hint_"), std::string::npos);
    EXPECT_EQ(source.find("Cancel: Stay"), std::string::npos);
    EXPECT_EQ(source.find("Cancel: Party"), std::string::npos);
    EXPECT_EQ(source.find("Cancel: Back"), std::string::npos);
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

    EXPECT_NE(source.find("formatBattleLogLines"), std::string::npos);
    EXPECT_NE(source.find("appendBattleLogLines"), std::string::npos);
    EXPECT_NE(source.find("BattleLogFormatterContext"), std::string::npos);
    EXPECT_EQ(source.find("formatActionResultText"), std::string::npos);
    EXPECT_EQ(source.find("formatRecoveryText"), std::string::npos);
    EXPECT_EQ(source.find("Result: Item used"), std::string::npos);
    EXPECT_EQ(source.find("dealt 0 dmg"), std::string::npos);
}

TEST(BattleSceneSmokeTest, AppendsBattleLogBeforeClearingPendingAction) {
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

    EXPECT_NE(header.find("#include \"game/battle/battle_log_formatter.h\""), std::string::npos);
    EXPECT_NE(header.find("struct BattleLogEntryViewModel"), std::string::npos);
    EXPECT_NE(header.find("battle_log_history_"), std::string::npos);
    EXPECT_NE(header.find("battle_log_entries_"), std::string::npos);
    EXPECT_NE(header.find("appendBattleLogLines"), std::string::npos);
    EXPECT_NE(header.find("rebuildBattleLogView"), std::string::npos);
    EXPECT_NE(header.find("battleLogToneClass"), std::string::npos);

    const std::string executing_snippet = snippetFrom(source, "last_action_result_ = session_.submitAction", 1200U);
    ASSERT_FALSE(executing_snippet.empty());
    const std::size_t append_pos = executing_snippet.find("appendBattleLogLines(game::battle::formatBattleLogLines");
    const std::size_t reset_pos = executing_snippet.find("pending_action_.reset()");
    ASSERT_NE(append_pos, std::string::npos);
    ASSERT_NE(reset_pos, std::string::npos);
    EXPECT_LT(append_pos, reset_pos);

    EXPECT_NE(source.find("BATTLE_LOG_HISTORY_LIMIT = 24U"), std::string::npos);
    EXPECT_NE(source.find("BATTLE_LOG_VISIBLE_LIMIT = 3U"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.markDirty(\"battle_log_entries\")"), std::string::npos);
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
    EXPECT_NE(source.find("std::clamp(16.0F * visual_scale, 12.0F, 42.0F)"), std::string::npos);
    EXPECT_NE(source.find("BattleShadowComponent"), std::string::npos);
    EXPECT_NE(source.find("syncPresentationShadows"), std::string::npos);
    EXPECT_NE(source.find("BATTLE_SHADOW_VERTICAL_PADDING"), std::string::npos);
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

TEST(BattleSceneSmokeTest, UsesBattleAnimationDirectorForResultPresentation) {
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

    EXPECT_NE(header.find("#include \"game/scene/battle_animation_director.h\""), std::string::npos);
    EXPECT_NE(header.find("BattleAnimationDirector battle_animation_director_{}"), std::string::npos);
    EXPECT_NE(header.find("collectBattlePresentationUnitAnchors"), std::string::npos);
    EXPECT_EQ(header.find("animation_timer_"), std::string::npos);

    const std::string executing_snippet = snippetFrom(source, "last_action_result_ = session_.submitAction", 1200U);
    ASSERT_FALSE(executing_snippet.empty());
    EXPECT_NE(executing_snippet.find("collectBattlePresentationUnitAnchors()"), std::string::npos);
    EXPECT_NE(executing_snippet.find("animationConfigForResult(*last_action_result_)"), std::string::npos);
    EXPECT_NE(executing_snippet.find("battle_animation_director_.begin(*last_action_result_, unit_anchors, animation_config)"),
              std::string::npos);

    const std::string animating_snippet = snippetFrom(source, "case FlowState::AnimatingResult:", 360U);
    ASSERT_FALSE(animating_snippet.empty());
    EXPECT_NE(animating_snippet.find("battle_animation_director_.update(delta_time)"), std::string::npos);
    EXPECT_NE(animating_snippet.find("battle_animation_director_.finished()"), std::string::npos);

    EXPECT_NE(source.find("previous_position_ = position"), std::string::npos);
    EXPECT_NE(source.find("pose->rotation_radians"), std::string::npos);
    EXPECT_NE(source.find("pose->color_multiplier"), std::string::npos);
    EXPECT_EQ(source.find("RESULT_HOLD_SECONDS"), std::string::npos);
    EXPECT_EQ(source.find("animation_timer_"), std::string::npos);
}

TEST(BattleSceneSmokeTest, ChoosesCastMotionForMagicSkillsAndGuard) {
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

    EXPECT_NE(header.find("animationConfigForResult"), std::string::npos);
    EXPECT_NE(header.find("actionMotionStyleForResult"), std::string::npos);
    EXPECT_NE(header.find("actionStartOffsetFor"), std::string::npos);

    const std::string style_snippet = snippetFrom(source, "BattleActionMotionStyle BattleScene::actionMotionStyleForResult", 1300U);
    ASSERT_FALSE(style_snippet.empty());
    EXPECT_NE(style_snippet.find("BattleActionType::Guard"), std::string::npos);
    EXPECT_NE(style_snippet.find("return BattleActionMotionStyle::Cast"), std::string::npos);
    EXPECT_NE(style_snippet.find("skill->hit_type_ == game::data::HitType::Physical"), std::string::npos);
    EXPECT_NE(style_snippet.find("return BattleActionMotionStyle::WeaponAttack"), std::string::npos);

    const std::string config_snippet = snippetFrom(source, "BattleAnimationTimelineConfig BattleScene::animationConfigForResult", 500U);
    ASSERT_FALSE(config_snippet.empty());
    EXPECT_NE(config_snippet.find("config.motion_style = actionMotionStyleForResult(result)"), std::string::npos);
    EXPECT_NE(config_snippet.find("config.actor_start_offset = actionStartOffsetFor(result.actor_id)"), std::string::npos);
}

TEST(BattleSceneSmokeTest, UsesDamagePopupControllerForResultNumbers) {
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

    EXPECT_NE(header.find("#include \"game/scene/battle_damage_popup_controller.h\""), std::string::npos);
    EXPECT_NE(header.find("BattleDamagePopupController battle_damage_popup_controller_{}"), std::string::npos);
    EXPECT_NE(header.find("collectBattlePresentationUnitAnchors"), std::string::npos);
    EXPECT_NE(header.find("renderDamagePopups"), std::string::npos);

    const std::string executing_snippet = snippetFrom(source, "last_action_result_ = session_.submitAction", 1200U);
    ASSERT_FALSE(executing_snippet.empty());
    EXPECT_NE(executing_snippet.find("collectBattlePresentationUnitAnchors()"), std::string::npos);
    EXPECT_NE(executing_snippet.find("battle_damage_popup_controller_.spawnFromResult(*last_action_result_, unit_anchors)"),
              std::string::npos);
    EXPECT_NE(executing_snippet.find("battle_animation_director_.begin(*last_action_result_, unit_anchors, animation_config)"),
              std::string::npos);

    EXPECT_NE(source.find("battle_damage_popup_controller_.update(delta_time)"), std::string::npos);
    EXPECT_NE(source.find("battle_damage_popup_controller_.clear()"), std::string::npos);
    EXPECT_NE(source.find("renderDamagePopups()"), std::string::npos);
    EXPECT_NE(source.find("void BattleScene::renderDamagePopups()"), std::string::npos);
    EXPECT_NE(source.find("DAMAGE_POPUP_FONT_SIZE_PX = 20"), std::string::npos);
    EXPECT_NE(source.find("resource_manager.loadFont(engine::resource::defaults::UI_DEFAULT_FONT_ID"), std::string::npos);
    EXPECT_NE(source.find("battleDamagePopupColor(popup.kind, popup.alpha)"), std::string::npos);
    EXPECT_NE(source.find("text_renderer.drawText"), std::string::npos);
}

TEST(BattleSceneSmokeTest, TriggersSkillTargetVfxFromAppliedSingleTargetSkillResults) {
    // 源码级回归：验证 guard 与调用顺序，运行时分支覆盖见后续单元测试。
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

    EXPECT_NE(source.find("#include \"engine/vfx/vfx_types.h\""), std::string::npos);
    EXPECT_NE(header.find("spawnSkillTargetVfx"), std::string::npos);
    EXPECT_NE(source.find("void BattleScene::spawnSkillTargetVfx"), std::string::npos);

    const std::string executing_snippet = snippetFrom(source, "last_action_result_ = session_.submitAction", 1400U);
    ASSERT_FALSE(executing_snippet.empty());
    EXPECT_NE(executing_snippet.find("const auto unit_anchors = collectBattlePresentationUnitAnchors()"),
              std::string::npos);
    EXPECT_NE(executing_snippet.find("spawnSkillTargetVfx(*last_action_result_, unit_anchors)"), std::string::npos);
    EXPECT_NE(executing_snippet.find("battle_damage_popup_controller_.spawnFromResult(*last_action_result_, unit_anchors)"),
              std::string::npos);
    EXPECT_LT(executing_snippet.find("spawnSkillTargetVfx(*last_action_result_, unit_anchors)"),
              executing_snippet.find("battle_animation_director_.begin(*last_action_result_, unit_anchors, animation_config)"));

    const std::string vfx_snippet = snippetFrom(source, "void BattleScene::spawnSkillTargetVfx", 1800U);
    ASSERT_FALSE(vfx_snippet.empty());
    EXPECT_NE(vfx_snippet.find("result.status != game::battle::BattleActionStatus::Applied"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("result.action_type != game::battle::BattleActionType::Skill"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("result.missed"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("!result.target_id"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("findUnitAnchor(unit_anchors, *result.target_id)"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("rpg_catalog_->findSkill(result.skill_id)"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("skill->target_vfx_id_hash_ == engine::vfx::kInvalidVfxEffectId"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("engine::vfx::PlayVfxCommand command{}"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("command.effect_id = skill->target_vfx_id_hash_"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("command.world_position = skillTargetVfxPosition(*target_anchor)"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("command.scale = skill->target_vfx_scale_"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("command.channel = engine::vfx::VfxChannel::Overlay"), std::string::npos);
    EXPECT_NE(vfx_snippet.find("context_.getDispatcher().trigger(command)"), std::string::npos);
}

TEST(BattleSceneSmokeTest, UsesEnemyHpBarControllerForEnemyHealthOverlay) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    const std::filesystem::path types_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene_types.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(types_path)) << types_path;

    const std::string header = readTextFile(header_path);
    const std::string source = readTextFile(source_path);
    const std::string types = readTextFile(types_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(types.empty());

    EXPECT_NE(types.find("#include \"game/scene/battle_enemy_hp_bar_controller.h\""), std::string::npos);
    EXPECT_NE(types.find("BattleEnemyHpBarConfig enemy_hp_bar_config{}"), std::string::npos);
    EXPECT_NE(header.find("BattleEnemyHpBarController battle_enemy_hp_bar_controller_{}"), std::string::npos);
    EXPECT_NE(header.find("syncEnemyHpBarHighlight"), std::string::npos);
    EXPECT_NE(header.find("renderEnemyHpBars"), std::string::npos);

    const std::string constructor_snippet = snippetFrom(source, "BattleScene::BattleScene", 1100U);
    ASSERT_FALSE(constructor_snippet.empty());
    EXPECT_NE(constructor_snippet.find("battle_enemy_hp_bar_controller_(presentation_options_.enemy_hp_bar_config)"),
              std::string::npos);

    const std::string executing_snippet = snippetFrom(source, "last_action_result_ = session_.submitAction", 720U);
    ASSERT_FALSE(executing_snippet.empty());
    EXPECT_NE(executing_snippet.find("battle_enemy_hp_bar_controller_.syncFromSnapshot(last_action_result_->snapshot)"),
              std::string::npos);
    EXPECT_NE(executing_snippet.find("battle_enemy_hp_bar_controller_.revealFromResult(*last_action_result_)"),
              std::string::npos);

    EXPECT_NE(source.find("battle_enemy_hp_bar_controller_.syncFromSnapshot(session_.snapshot())"),
              std::string::npos);

    EXPECT_NE(source.find("battle_enemy_hp_bar_controller_.update(delta_time)"), std::string::npos);
    EXPECT_NE(source.find("battle_enemy_hp_bar_controller_.clear()"), std::string::npos);
    EXPECT_NE(source.find("syncEnemyHpBarHighlight()"), std::string::npos);
    EXPECT_NE(source.find("renderEnemyHpBars()"), std::string::npos);
    EXPECT_NE(source.find("void BattleScene::renderEnemyHpBars()"), std::string::npos);
    EXPECT_NE(source.find("HP_BAR_WARNING_RATIO = 0.50F"), std::string::npos);
    EXPECT_NE(source.find("HP_BAR_DANGER_RATIO = 0.25F"), std::string::npos);
    EXPECT_NE(source.find("enemyHpBarScreenTopLeft"), std::string::npos);
}

TEST(BattleSceneSmokeTest, CurrentPlayerActorUsesCommandFocusPoseWhileWaitingForInput) {
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

    EXPECT_NE(header.find("commandFocusPoseFor"), std::string::npos);
    EXPECT_NE(header.find("presentationPoseFor"), std::string::npos);
    EXPECT_NE(header.find("command_focus_actor_id_"), std::string::npos);
    EXPECT_NE(header.find("command_focus_elapsed_seconds_"), std::string::npos);
    EXPECT_NE(source.find("constexpr glm::vec2 COMMAND_FOCUS_PLAYER_OFFSET{-12.0F, -2.0F}"), std::string::npos);
    EXPECT_NE(source.find("constexpr float COMMAND_FOCUS_EASE_SECONDS = 0.18F"), std::string::npos);
    EXPECT_NE(source.find("updateCommandFocus(delta_time)"), std::string::npos);
    EXPECT_NE(source.find("state_ != FlowState::WaitingForInput"), std::string::npos);
    EXPECT_NE(source.find("battle_animation_director_.active()"), std::string::npos);
    EXPECT_NE(source.find("side != game::battle::BattleSide::Player"), std::string::npos);
    EXPECT_NE(source.find("pose.offset = COMMAND_FOCUS_PLAYER_OFFSET * eased"), std::string::npos);
    EXPECT_NE(source.find("return commandFocusPoseFor(unit_id, side)"), std::string::npos);
}

TEST(BattleSceneSmokeTest, BattlePresentationFreezesKoAnimationAndSortsShadowEntities) {
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;

    const std::string scene_source = readTextFile(scene_source_path);
    ASSERT_FALSE(scene_source.empty());

    EXPECT_NE(scene_source.find("struct BattleShadowComponent"), std::string::npos);
    EXPECT_NE(scene_source.find("if (!unit || !unit->isAlive())"), std::string::npos);
    EXPECT_NE(scene_source.find("continue;\n        }\n\n        auto& animation"), std::string::npos);
    EXPECT_NE(scene_source.find("battle_registry_.emplace<BattleShadowComponent>(shadow_entity"), std::string::npos);
    EXPECT_NE(scene_source.find("syncPresentationShadows()"), std::string::npos);
    EXPECT_NE(scene_source.find("shadow_render->depth_ = sprite.depth + pose_depth_offset + BATTLE_SHADOW_DEPTH_OFFSET"),
              std::string::npos);
    EXPECT_NE(scene_source.find("target_render->depth_ = sprite.depth + pose_depth_offset + BATTLE_TARGET_SHADOW_DEPTH_OFFSET"),
              std::string::npos);
    EXPECT_EQ(scene_source.find("renderer.drawFilledEllipse"), std::string::npos);
    EXPECT_EQ(scene_source.find("glm::vec2{sprite.shadow_size.x, 4.0F}"), std::string::npos);
}

TEST(BattleSceneSmokeTest, RmlUsesDataDrivenBattleMenuBindings) {
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/battle.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string rml = readTextFile(rml_path);
    ASSERT_FALSE(rml.empty());

    EXPECT_NE(rml.find("../theme/nav.rcss"), std::string::npos);
    EXPECT_NE(rml.find("../theme/spritesheet.rcss"), std::string::npos);
    EXPECT_NE(rml.find("../theme/portrait.rcss"), std::string::npos);
    EXPECT_NE(rml.find("../theme/battle_enemy_icons.rcss"), std::string::npos);
    EXPECT_NE(rml.find("../theme/battle_state_icons.rcss"), std::string::npos);
    EXPECT_NE(rml.find("tf-screen-root tf-nav-root"), std::string::npos);
    EXPECT_EQ(rml.find("Battle Prototype"), std::string::npos);
    EXPECT_EQ(rml.find("tf-button-secondary"), std::string::npos);
    EXPECT_EQ(rml.find("tf-button-primary"), std::string::npos);
    EXPECT_EQ(rml.find("<progress"), std::string::npos);
    EXPECT_GE(countOccurrences(rml, "tf-nav-auto"), 3U);
    EXPECT_NE(rml.find("id=\"battle-top-status\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-turn-order-bar\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-log-panel\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-victory-overlay\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"victory_overlay_visible\""), std::string::npos);
    EXPECT_NE(rml.find("{{ victory_gold_text }}"), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"victory_items_empty\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"item : victory_reward_items\""), std::string::npos);
    EXPECT_NE(rml.find("data-style-decorator=\"item.icon_decorator\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-victory-continue\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"victory_continue\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"entry : battle_log_entries\""), std::string::npos);
    EXPECT_NE(rml.find("{{ entry.text }}"), std::string::npos);
    EXPECT_NE(rml.find("data-class-log-damage=\"entry.tone_class == 'damage'\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-log-recovery=\"entry.tone_class == 'recovery'\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-log-state=\"entry.tone_class == 'state'\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-log-system=\"entry.tone_class == 'system'\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-log-error=\"entry.tone_class == 'error'\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"entry : turn_order_entries\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-current-turn-entry=\"entry.current\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-acted-turn-entry=\"entry.acted\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-ko-turn-entry=\"entry.ko\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-enemy-turn-entry=\"entry.enemy\""), std::string::npos);
    EXPECT_NE(rml.find("data-style-decorator=\"entry.portrait_decorator\""), std::string::npos);
    EXPECT_NE(rml.find("{{ entry.short_label }}"), std::string::npos);
    EXPECT_NE(rml.find("battle-turn-order-badge"), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"entry.badge_label != ''\""), std::string::npos);
    EXPECT_NE(rml.find("{{ entry.badge_label }}"), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-turn\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-result\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"member : party_status\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"icon : party_state_icons\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"icon.unit_id == member.unit_id\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-mouseover=\"state_icon_hover_enter(icon.unit_id, icon.entry_index)\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-mouseout=\"state_icon_hover_exit(icon.unit_id, icon.entry_index)\""), std::string::npos);
    EXPECT_NE(rml.find("data-style-decorator=\"icon.icon_decorator\""), std::string::npos);
    EXPECT_NE(rml.find("{{ icon.turns_text }}"), std::string::npos);
    EXPECT_NE(rml.find("battle-state-tooltip"), std::string::npos);
    EXPECT_NE(rml.find("state_tooltip.active_unit_id == member.unit_id"), std::string::npos);
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
    EXPECT_EQ(rml.find("battle-back-hint"), std::string::npos);
    EXPECT_EQ(rml.find("back_hint"), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-party-command\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"party_command_visible\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"command : party_commands\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"party_command_select(command.entry_index)\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-actor-command\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"actor_command_visible\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"command : actor_commands\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"actor_command_select(command.entry_index)\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"list_menu_visible\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"entry : list_entries\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"target_menu_visible\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"target : target_entries\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"target_entry_select(target.entry_index)\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-is-ally=\"target.is_ally\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-is-dead=\"target.is_dead\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-disabled=\"!target.enabled\""), std::string::npos);
    EXPECT_EQ(rml.find("data-for=\"action : main_actions\""), std::string::npos);
    EXPECT_EQ(rml.find("data-event-click=\"main_action_select(action.entry_index)\""), std::string::npos);
}

TEST(BattleSceneSmokeTest, RcssDefinesStage5BattleMenuStates) {
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/battle.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;

    const std::string rcss = readTextFile(rcss_path);
    ASSERT_FALSE(rcss.empty());

    EXPECT_NE(rcss.find("top: 256dp;"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-log-panel"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-victory-overlay"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-victory-panel"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-victory-continue"), std::string::npos);
    EXPECT_NE(rcss.find("tab-index: auto;"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-victory-item-icon"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-log-entry"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-log-entry.log-damage"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-log-entry.log-recovery"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-log-entry.log-state"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-log-entry.log-system"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-log-entry.log-error"), std::string::npos);
    EXPECT_NE(rcss.find("top: 42dp;"), std::string::npos);
    const std::string log_panel_block = snippetFrom(rcss, "#battle-log-panel {");
    ASSERT_FALSE(log_panel_block.empty());
    EXPECT_NE(log_panel_block.find("left: 318dp;"), std::string::npos);
    EXPECT_NE(log_panel_block.find("width: 314dp;"), std::string::npos);
    EXPECT_NE(log_panel_block.find("background-color: #00000000;"), std::string::npos);
    EXPECT_NE(log_panel_block.find("border-width: 0dp;"), std::string::npos);
    const std::string log_entry_block = snippetFrom(rcss, ".battle-log-entry {");
    ASSERT_FALSE(log_entry_block.empty());
    EXPECT_NE(log_entry_block.find("width: 314dp;"), std::string::npos);
    EXPECT_NE(log_entry_block.find("text-align: right;"), std::string::npos);
    EXPECT_EQ(rcss.find("top: 188dp;"), std::string::npos);
    EXPECT_EQ(rcss.find("height: 60dp;"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-turn-order-bar"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-turn-order-entry"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-turn-order-entry.current-turn-entry"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-turn-order-entry.acted-turn-entry"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-turn-order-entry.ko-turn-entry"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-turn-order-entry.enemy-turn-entry"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-turn-order-label"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-turn-order-badge"), std::string::npos);
    const std::string turn_entry_block = snippetFrom(rcss, ".battle-turn-order-entry {");
    ASSERT_FALSE(turn_entry_block.empty());
    EXPECT_NE(turn_entry_block.find("width: 30dp;"), std::string::npos);
    EXPECT_NE(turn_entry_block.find("height: 30dp;"), std::string::npos);
    EXPECT_NE(turn_entry_block.find("padding: 0;"), std::string::npos);
    EXPECT_NE(turn_entry_block.find("border-width: 0dp;"), std::string::npos);

    const std::string turn_portrait_block = snippetFrom(rcss, ".battle-turn-order-portrait {");
    ASSERT_FALSE(turn_portrait_block.empty());
    EXPECT_NE(turn_portrait_block.find("width: 28dp;"), std::string::npos);
    EXPECT_NE(turn_portrait_block.find("height: 28dp;"), std::string::npos);

    const std::string turn_badge_block = snippetFrom(rcss, ".battle-turn-order-badge {");
    ASSERT_FALSE(turn_badge_block.empty());
    EXPECT_NE(turn_badge_block.find("left: 20dp;"), std::string::npos);
    EXPECT_NE(turn_badge_block.find("top: 20dp;"), std::string::npos);
    EXPECT_NE(rcss.find("image-color: #ffffffff;"), std::string::npos);
    EXPECT_NE(rcss.find("image-color: #ffffff77;"), std::string::npos);
    EXPECT_NE(rcss.find("image-color: #ffffff44;"), std::string::npos);
    EXPECT_NE(rcss.find("font-effect: shadow(1dp 1dp #000000cc);"), std::string::npos);
    EXPECT_NE(rcss.find("overflow: hidden;"), std::string::npos);
    EXPECT_NE(rcss.find("height: 104dp;"), std::string::npos);
    EXPECT_NE(rcss.find("left: 464dp;"), std::string::npos);
    EXPECT_NE(rcss.find("width: 168dp;"), std::string::npos);
    EXPECT_EQ(rcss.find("width: 302dp"), std::string::npos);
    EXPECT_EQ(rcss.find("#battle-menu-title"), std::string::npos);
    EXPECT_EQ(rcss.find("#battle-menu-hint"), std::string::npos);
    EXPECT_EQ(rcss.find("#battle-back-hint"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-list-entry.disabled"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-text-button"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-party-command"), std::string::npos);
    EXPECT_NE(rcss.find("#battle-actor-command"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-party-command-button"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-actor-command-button"), std::string::npos);
    EXPECT_EQ(rcss.find("#battle-main-actions"), std::string::npos);
    EXPECT_EQ(rcss.find(".battle-action-button"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-hp-fill"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-mp-fill"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-state-icon-row"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-state-icon-image"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-state-turns"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-state-tooltip"), std::string::npos);
    EXPECT_NE(rcss.find(".battle-party-card.ko-party-member .battle-state-icon-row"), std::string::npos);
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

TEST(BattleSceneSmokeTest, StateIconSpritesheetDefinesProjectStateFrames) {
    const std::filesystem::path icon_rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/theme/battle_state_icons.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(icon_rcss_path)) << icon_rcss_path;

    const std::string icon_rcss = readTextFile(icon_rcss_path);
    ASSERT_FALSE(icon_rcss.empty());

    EXPECT_NE(icon_rcss.find("@spritesheet battle-state-icons-potions"), std::string::npos);
    EXPECT_NE(icon_rcss.find("@spritesheet battle-state-icons-materials"), std::string::npos);
    EXPECT_NE(icon_rcss.find("battle-state-icon-poison"), std::string::npos);
    EXPECT_NE(icon_rcss.find("battle-state-icon-stun"), std::string::npos);
    EXPECT_NE(icon_rcss.find("battle-state-icon-burn"), std::string::npos);
    EXPECT_NE(icon_rcss.find("battle-state-icon-fallback"), std::string::npos);
}

TEST(BattleSceneSmokeTest, EnemyIconSpritesheetDefinesProjectEnemyFrames) {
    const std::filesystem::path icon_rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/theme/battle_enemy_icons.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(icon_rcss_path)) << icon_rcss_path;

    const std::string icon_rcss = readTextFile(icon_rcss_path);
    ASSERT_FALSE(icon_rcss.empty());

    EXPECT_NE(icon_rcss.find("@spritesheet battle-enemy-goblin-icons"), std::string::npos);
    EXPECT_NE(icon_rcss.find("@spritesheet battle-enemy-gnome-icons"), std::string::npos);
    EXPECT_NE(icon_rcss.find("@spritesheet battle-enemy-slime-icons"), std::string::npos);
    EXPECT_NE(icon_rcss.find("battle-enemy-icon-goblin: 0px 0px 32px 32px;"), std::string::npos);
    EXPECT_NE(icon_rcss.find("battle-enemy-icon-gnome: 0px 0px 32px 32px;"), std::string::npos);
    EXPECT_NE(icon_rcss.find("battle-enemy-icon-slime: 0px 32px 32px 32px;"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
