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

} // namespace

namespace game::scene {
namespace {

TEST(GameSceneBattleEntryTest, ConnectsAndDisconnectsBattleDispatcherHooks) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("sink<game::defs::EnterBattleCommand>().connect<&GameScene::onEnterBattleCommand>(this);"),
              std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::EnterBattleCommand>().disconnect<&GameScene::onEnterBattleCommand>(this);"),
              std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::BattleEndedEvent>().connect<&GameScene::onBattleEnded>(this);"),
              std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::BattleEndedEvent>().disconnect<&GameScene::onBattleEnded>(this);"),
              std::string::npos);
}

TEST(GameSceneBattleEntryTest, PushesBattleSceneOnEnterBattleCommand) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("requestPushScene(std::make_unique<game::scene::BattleScene>("),
              std::string::npos);
}

TEST(GameSceneBattleEntryTest, PassesVfxServiceToBattleScenePresentationOptions) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("presentation_options.vfx_service = services_->vfx_service.get();"),
              std::string::npos);
}

TEST(GameSceneBattleEntryTest, SupportsCatalogFallbackWhenCommandUnitsAreEmpty) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("if (units.empty())"), std::string::npos);
    EXPECT_NE(source.find("buildBattleUnitsFromCatalog"), std::string::npos);
    EXPECT_NE(source.find("services_->rpg_catalog"), std::string::npos);
}

TEST(GameSceneBattleEntryTest, SupportsTroopAndActorSelectionInBattleCommand) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("cmd.troop_id"), std::string::npos);
    EXPECT_NE(source.find("cmd.actor_ids"), std::string::npos);
    EXPECT_NE(source.find("resolveBattleActorIds"), std::string::npos);
    EXPECT_NE(source.find("PartyComponent"), std::string::npos);
}

TEST(GameSceneBattleEntryTest, GuardsAgainstNestedBattleEntries) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = readTextFile(header_path);
    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());

    EXPECT_NE(header.find("bool battle_in_progress_{false};"), std::string::npos);
    EXPECT_NE(source.find("if (battle_in_progress_ || active_encounter_context_)"), std::string::npos);
    EXPECT_NE(source.find("releaseEnemyEncounterEntryFailure(*cmd.encounter_context);"), std::string::npos);
    EXPECT_NE(source.find("battle_in_progress_ = true;"), std::string::npos);
    EXPECT_NE(source.find("battle_in_progress_ = false;"), std::string::npos);
}

TEST(GameSceneBattleEntryTest, BattleStartedEventUsesResolvedPlayerActorIds) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("std::vector<std::string> battle_actor_ids = cmd.actor_ids;"), std::string::npos);
    EXPECT_NE(source.find("collectPlayerActorIdsFromUnits(units)"), std::string::npos);
    EXPECT_NE(source.find("battle_actor_ids = resolveBattleActorIds(registry_, cmd.actor_ids);"), std::string::npos);
    EXPECT_NE(source.find(".actor_ids = battle_actor_ids,"), std::string::npos);
    EXPECT_EQ(source.find(".actor_ids = cmd.actor_ids,"), std::string::npos);
}

TEST(GameSceneBattleEntryTest, ResolvesEnemyEncounterBeforeBattleRewardSettlement) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    const auto resolve_pos = source.find("resolveActiveEnemyEncounter(evt);");
    const auto settlement_pos = source.find("processBattleEndedForGameScene(");
    ASSERT_NE(resolve_pos, std::string::npos);
    ASSERT_NE(settlement_pos, std::string::npos);
    EXPECT_LT(resolve_pos, settlement_pos);
    EXPECT_NE(source.find("active_encounter_context_ = cmd.encounter_context;"), std::string::npos);
    EXPECT_NE(source.find("context.respawn_on_map_reload"), std::string::npos);
    EXPECT_NE(source.find("defeated_encounters.erase(context.encounter_id);"), std::string::npos);
    EXPECT_NE(source.find("defeated_encounters.insert(context.encounter_id);"), std::string::npos);
    EXPECT_NE(source.find("NeedRemoveTag"), std::string::npos);
}

TEST(GameSceneBattleEntryTest, BattleRuntimeStatsWritebackPublishesPartyStatsChanged) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("BattleRuntimeStatsWritebackResult"), std::string::npos);
    EXPECT_NE(source.find("const auto runtime_stats_writeback = writeBackBattleRuntimeStats"), std::string::npos);
    EXPECT_NE(source.find("runtime_stats_writeback.changed"), std::string::npos);
    EXPECT_NE(source.find("game::defs::PartyRuntimeStatsChanged"), std::string::npos);
    EXPECT_NE(source.find(".full_sync = true"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
