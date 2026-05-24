#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "engine/script/script_host.h"
#include "game/component/party_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands_battle.h"
#include "game/defs/commands_quest.h"
#include "game/defs/commands_recruit.h"
#include "game/defs/commands_shop.h"
#include "game/runtime/rpg_catalog_loader.h"
#include "game/world/world_state.h"
#include "script_test_utils.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <string>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

constexpr std::string_view QUEST_ID = "quest.test.hunter_trial";
constexpr std::string_view ACTOR_ID = "actor.lyria";
constexpr std::string_view SHOP_ID = "shop.village.general";
constexpr std::string_view TROOP_ID = "troop.slime";

[[nodiscard]] std::filesystem::path projectRoot() {
    return std::filesystem::path{PROJECT_SOURCE_DIR};
}

[[nodiscard]] game::data::QuestCatalog loadQuestCatalog() {
    game::data::QuestCatalog catalog{};
    EXPECT_TRUE(catalog.loadFromFile((projectRoot() / "tests/data/quest_interaction_quests.json").string()));
    return catalog;
}

[[nodiscard]] game::data::ItemCatalog loadItemCatalog() {
    game::data::ItemCatalog catalog{};
    EXPECT_TRUE(catalog.loadItemConfig((projectRoot() / "assets/data/item_config.json").string()));
    return catalog;
}

[[nodiscard]] game::data::ShopCatalog loadShopCatalog() {
    game::data::ShopCatalog catalog{};
    EXPECT_TRUE(catalog.loadFromFile((projectRoot() / "assets/data/shops.json").string()));
    return catalog;
}

[[nodiscard]] game::data::RpgCatalog loadRpgCatalog(const game::data::ItemCatalog& item_catalog) {
    game::data::RpgCatalog catalog{};
    std::string error{};
    EXPECT_TRUE(game::runtime::loadRpgCatalogFromManifest(
        catalog,
        game::runtime::RpgCatalogLoadOptions{
            .manifest_path = (projectRoot() / "assets/data/rpg/manifest.json").string(),
            .root_path = (projectRoot() / "assets/data/rpg").string(),
            .item_catalog = &item_catalog,
        },
        error)) << error;
    return catalog;
}

struct CommandCapture {
    std::vector<game::defs::AcceptQuestCommand> accept_quests{};
    std::vector<game::defs::TurnInQuestCommand> turn_in_quests{};
    std::vector<game::defs::RecruitPartyMemberCommand> recruits{};
    std::vector<game::defs::OpenShopCommand> open_shops{};
    std::vector<game::defs::EnterBattleCommand> battles{};

    void onAcceptQuest(const game::defs::AcceptQuestCommand& command) {
        accept_quests.push_back(command);
    }

    void onTurnInQuest(const game::defs::TurnInQuestCommand& command) {
        turn_in_quests.push_back(command);
    }

    void onRecruit(const game::defs::RecruitPartyMemberCommand& command) {
        recruits.push_back(command);
    }

    void onOpenShop(const game::defs::OpenShopCommand& command) {
        open_shops.push_back(command);
    }

    void onBattle(const game::defs::EnterBattleCommand& command) {
        battles.push_back(command);
    }
};

struct ScriptPhase2ApiEnv {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::data::QuestCatalog quest_catalog{loadQuestCatalog()};
    game::data::ItemCatalog item_catalog{loadItemCatalog()};
    game::data::ShopCatalog shop_catalog{loadShopCatalog()};
    game::data::RpgCatalog rpg_catalog{loadRpgCatalog(item_catalog)};
    game::world::WorldState world_state{};
    engine::script::ScriptHost host;
    entt::entity player{entt::null};
    entt::entity giver{entt::null};
    entt::entity recruiter{entt::null};
    entt::entity merchant{entt::null};

    ScriptPhase2ApiEnv()
        : host(registry) {
        registry.ctx().emplace<game::data::QuestCatalog*>(&quest_catalog);
        registry.ctx().emplace<game::data::RpgCatalog*>(&rpg_catalog);
        registry.ctx().emplace<game::data::ShopCatalog*>(&shop_catalog);

        const entt::id_type map_id = world_state.ensureExternalMap("home_exterior");
        world_state.setCurrentMap(map_id);
        registry.ctx().emplace<game::world::WorldState*>(&world_state);

        player = registry.create();
        registry.emplace<game::component::PlayerTag>(player);
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});
        registry.emplace<game::component::QuestLogComponent>(player);
        registry.emplace<game::component::PartyComponent>(
            player,
            game::component::PartyComponent{
                .recruited_actor_ids_ = {"actor.player", std::string{ACTOR_ID}},
                .active_actor_ids_ = {"actor.player", std::string{ACTOR_ID}},
                .max_active_members_ = 4,
            });
        registry.emplace<game::component::PartyRuntimeStatsComponent>(
            player,
            game::component::PartyRuntimeStatsComponent{
                .states_by_actor_id_ = {
                    {std::string{ACTOR_ID}, game::component::ActorRuntimeState{.level = 7}},
                },
                .revision_ = 1,
            });

        giver = registry.create();
        registry.emplace<game::component::QuestGiverComponent>(
            giver,
            game::component::QuestGiverComponent{
                .quest_id_ = std::string{QUEST_ID},
                .quest_id_hash_ = entt::hashed_string{QUEST_ID.data(), QUEST_ID.size()}.value(),
            });

        recruiter = registry.create();
        merchant = registry.create();

        EXPECT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));
        host.luaState()["test_giver"] = host.makeHandle(giver);
        host.luaState()["test_recruiter"] = host.makeHandle(recruiter);
        host.luaState()["test_merchant"] = host.makeHandle(merchant);
    }
};

} // namespace

namespace game::script {

TEST(ScriptPhase2ApiTest, QuestPartyAndMapQueriesExposeGameplayState) {
    ScriptPhase2ApiEnv env{};

    EXPECT_TRUE(env.host.exec(R"(
        assert(tf.quest.status("quest.test.hunter_trial") == "offerable")
        assert(tf.quest.is_available("quest.test.hunter_trial") == true)
        local progress = tf.quest.progress("quest.test.hunter_trial", "slime_hunt")
        assert(progress.current == 0)
        assert(progress.required == 2)

        local members = tf.party.members()
        assert(#members == 2)
        assert(members[1] == "actor.player")
        assert(members[2] == "actor.lyria")
        assert(tf.party.is_recruited("actor.lyria") == true)
        assert(tf.party.is_recruited("actor.tori") == false)
        assert(tf.party.level("actor.lyria") == 7)
        assert(tf.party.level("actor.tori") == 0)
        assert(tf.party.initial_level("actor.tori") == 1)
        assert(tf.map.current() == "home_exterior")
    )"));

    auto& quest_log = env.registry.get<game::component::QuestLogComponent>(env.player);
    quest_log.active_quests.push_back(std::string{QUEST_ID});
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "slime_hunt")] = 1;
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "bat_hunt")] = 0;

    EXPECT_TRUE(env.host.exec(R"(
        assert(tf.quest.status("quest.test.hunter_trial") == "in_progress")
        local progress = tf.quest.progress("quest.test.hunter_trial", "slime_hunt")
        assert(progress.current == 1)
        assert(progress.required == 2)
    )"));

    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "slime_hunt")] = 2;
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "bat_hunt")] = 1;

    EXPECT_TRUE(env.host.exec(R"(
        assert(tf.quest.status("quest.test.hunter_trial") == "ready_to_turn_in")
    )"));
}

TEST(ScriptPhase2ApiTest, TriggerApisEmitTypedCommands) {
    ScriptPhase2ApiEnv env{};
    CommandCapture capture{};
    env.dispatcher.sink<game::defs::AcceptQuestCommand>().connect<&CommandCapture::onAcceptQuest>(&capture);
    env.dispatcher.sink<game::defs::TurnInQuestCommand>().connect<&CommandCapture::onTurnInQuest>(&capture);
    env.dispatcher.sink<game::defs::RecruitPartyMemberCommand>().connect<&CommandCapture::onRecruit>(&capture);
    env.dispatcher.sink<game::defs::OpenShopCommand>().connect<&CommandCapture::onOpenShop>(&capture);
    env.dispatcher.sink<game::defs::EnterBattleCommand>().connect<&CommandCapture::onBattle>(&capture);

    EXPECT_TRUE(env.host.exec(R"(
        local accept = tf.quest.accept("quest.test.hunter_trial", test_giver)
        assert(accept.ok == true)
        assert(accept.reason == "")

        local recruit = tf.party.request_recruit("actor.lyria", test_recruiter)
        assert(recruit.ok == true)

        local shop = tf.shop.open("shop.village.general", test_merchant)
        assert(shop.ok == true)

        local battle = tf.battle.start("troop.slime", {
            actor_ids = {"actor.lyria"},
            battle_background_id = "Grassland"
        })
        assert(battle.ok == true)
    )"));

    ASSERT_EQ(capture.accept_quests.size(), 1U);
    EXPECT_EQ(capture.accept_quests.front().quest_id, QUEST_ID);
    EXPECT_EQ(capture.accept_quests.front().giver, env.giver);

    ASSERT_EQ(capture.recruits.size(), 1U);
    EXPECT_EQ(capture.recruits.front().actor_id, ACTOR_ID);
    EXPECT_EQ(capture.recruits.front().recruiter, env.recruiter);

    ASSERT_EQ(capture.open_shops.size(), 1U);
    EXPECT_EQ(capture.open_shops.front().shop_id, SHOP_ID);
    EXPECT_EQ(capture.open_shops.front().merchant, env.merchant);

    ASSERT_EQ(capture.battles.size(), 1U);
    EXPECT_EQ(capture.battles.front().troop_id, TROOP_ID);
    ASSERT_EQ(capture.battles.front().actor_ids.size(), 1U);
    EXPECT_EQ(capture.battles.front().actor_ids.front(), ACTOR_ID);
    EXPECT_EQ(capture.battles.front().battle_background_id, "Grassland");

    auto& quest_log = env.registry.get<game::component::QuestLogComponent>(env.player);
    quest_log.active_quests.push_back(std::string{QUEST_ID});
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "slime_hunt")] = 2;
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "bat_hunt")] = 1;

    EXPECT_TRUE(env.host.exec(R"(
        local turn_in = tf.quest.turn_in("quest.test.hunter_trial", test_giver)
        assert(turn_in.ok == true)
    )"));

    ASSERT_EQ(capture.turn_in_quests.size(), 1U);
    EXPECT_EQ(capture.turn_in_quests.front().quest_id, QUEST_ID);
    EXPECT_EQ(capture.turn_in_quests.front().giver, env.giver);
}

TEST(ScriptPhase2ApiTest, TriggerApisReturnFailureReasonsBeforeDispatch) {
    ScriptPhase2ApiEnv env{};

    EXPECT_TRUE(env.host.exec(R"(
        local accept = tf.quest.accept("quest.missing", test_giver)
        assert(accept.ok == false)
        assert(accept.reason == "unknown_quest")

        local turn_in = tf.quest.turn_in("quest.test.hunter_trial", test_giver)
        assert(turn_in.ok == false)
        assert(turn_in.reason == "not_ready")

        local recruit = tf.party.request_recruit("actor.missing", test_recruiter)
        assert(recruit.ok == false)
        assert(recruit.reason == "unknown_actor")

        local shop = tf.shop.open("shop.missing", test_merchant)
        assert(shop.ok == false)
        assert(shop.reason == "unknown_shop")

        local battle = tf.battle.start("troop.missing")
        assert(battle.ok == false)
        assert(battle.reason == "unknown_troop")

        local empty_battle = tf.battle.start()
        assert(empty_battle.ok == false)
        assert(empty_battle.reason == "invalid_troop_id")
    )"));
}

} // namespace game::script
