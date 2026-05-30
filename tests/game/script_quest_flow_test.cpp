#include <gtest/gtest.h>

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "engine/script/script_host.h"
#include "game/battle/battle_unit_factory.h"
#include "game/component/actor_identity_component.h"
#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands_quest.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/events_dialogue.h"
#include "game/defs/events_quest.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_battle_progress_resolver.h"
#include "game/domain/quest_turn_in_service.h"
#include "game/script/script_event_bridge.h"
#include "game/system/quest_interaction_system.h"
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

constexpr std::string_view QUEST_ID = "quest.village.goblin_cleanup";
constexpr std::string_view QUEST_OBJECTIVE_ID = "kill_slimes";

[[nodiscard]] std::filesystem::path projectRoot() {
    return std::filesystem::path{PROJECT_SOURCE_DIR};
}

[[nodiscard]] std::string projectScriptRoot() {
    return (projectRoot() / "scripts").string();
}

[[nodiscard]] game::data::QuestCatalog loadQuestCatalog() {
    game::data::QuestCatalog catalog{};
    EXPECT_TRUE(catalog.loadFromFile((projectRoot() / "assets/data/quests.json").string()));
    return catalog;
}

[[nodiscard]] game::data::ItemCatalog loadItemCatalog() {
    game::data::ItemCatalog catalog{};
    EXPECT_TRUE(catalog.loadItemConfig((projectRoot() / "assets/data/item_config.json").string()));
    return catalog;
}

[[nodiscard]] game::data::RpgCatalog loadRpgCatalog() {
    const auto rpg_root = projectRoot() / "assets/data/rpg";
    game::data::RpgCatalog catalog{};
    EXPECT_TRUE(catalog.loadManifest((rpg_root / "manifest.json").string()));
    EXPECT_TRUE(catalog.loadClasses((rpg_root / "classes.json").string()));
    EXPECT_TRUE(catalog.loadActors((rpg_root / "actors.json").string()));
    EXPECT_TRUE(catalog.loadSkills((rpg_root / "skills.json").string()));
    EXPECT_TRUE(catalog.loadStates((rpg_root / "states.json").string()));
    EXPECT_TRUE(catalog.loadEquipment((rpg_root / "equipment.json").string()));
    EXPECT_TRUE(catalog.loadEnemies((rpg_root / "enemies.json").string()));
    EXPECT_TRUE(catalog.loadTroops((rpg_root / "troops.json").string()));
    return catalog;
}

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};
    int hide_count{0};

    void onShow(const game::defs::DialogueShowEvent& event) {
        shows.push_back(event);
    }

    void onHide(const game::defs::DialogueHideEvent&) {
        ++hide_count;
    }
};

struct QuestOfferCapture {
    std::vector<game::defs::QuestOfferRequestedEvent> requests{};

    void onRequest(const game::defs::QuestOfferRequestedEvent& event) {
        requests.push_back(event);
    }
};

struct ScriptQuestFlowEnv {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::data::QuestCatalog quest_catalog{loadQuestCatalog()};
    game::data::ItemCatalog item_catalog{loadItemCatalog()};
    game::data::RpgCatalog rpg_catalog{loadRpgCatalog()};
    game::domain::InventoryDomainService inventory_domain_service;
    game::domain::QuestTurnInService turn_in_service;
    game::system::QuestInteractionSystem quest_interaction_system;
    engine::script::ScriptHost host;
    game::script::ScriptEventBridge bridge;
    DialogueCapture capture{};
    entt::entity player{entt::null};
    entt::entity giver{entt::null};

    ScriptQuestFlowEnv()
        : inventory_domain_service(registry, dispatcher, item_catalog),
          turn_in_service(registry, item_catalog, inventory_domain_service),
          quest_interaction_system(registry, dispatcher, quest_catalog, turn_in_service),
          host(registry),
          bridge(host, registry, dispatcher) {
        registry.ctx().emplace<game::data::QuestCatalog*>(&quest_catalog);

        player = registry.create();
        registry.emplace<game::component::PlayerTag>(player);
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});
        registry.emplace<game::component::QuestLogComponent>(player);
        registry.emplace<game::component::InventoryComponent>(player);
        registry.emplace<game::component::PlayerWalletComponent>(player);

        giver = registry.create();
        registry.emplace<engine::component::TransformComponent>(giver, glm::vec2{24.0F, 0.0F});
        registry.emplace<engine::component::NameComponent>(
            giver,
            engine::component::NameComponent{
                .name_id_ = entt::hashed_string{"Manu"}.value(),
                .name_ = "Manu",
            });
        registry.emplace<game::component::ActorIdentityComponent>(
            giver,
            game::component::ActorIdentityComponent{
                .actor_id_ = "npc.manu",
                .actor_id_hash_ = entt::hashed_string{"npc.manu"}.value(),
                .blueprint_id_ = "quest",
            });
        registry.emplace<game::component::QuestGiverComponent>(
            giver,
            game::component::QuestGiverComponent{
                .quest_id_ = std::string{QUEST_ID},
                .quest_id_hash_ = entt::hashed_string{QUEST_ID.data(), QUEST_ID.size()}.value(),
            });
        registry.emplace<game::component::ScriptedInteractionComponent>(giver);

        dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);
        dispatcher.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&capture);

        host.setScriptRoot(projectScriptRoot());
        EXPECT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));
    }

    ~ScriptQuestFlowEnv() {
        dispatcher.disconnect(&capture);
    }

    void loadQuestScript() {
        EXPECT_TRUE(host.exec(R"(
            tf.script.require("lib.dialogue")
            local quest = tf.script.require("lib.quest")
            assert(tf.script.require(quest.module_for("quest.village.goblin_cleanup")) ~= nil)
        )"));
    }

    void interact() {
        dispatcher.trigger(game::defs::InteractCommand{player, giver});
        bridge.drainDeferredCommands();
        dispatcher.update();
    }

    [[nodiscard]] game::component::QuestLogComponent& questLog() {
        return registry.get<game::component::QuestLogComponent>(player);
    }
};

void defeatProjectSlimeTroop(ScriptQuestFlowEnv& env) {
    std::vector<game::battle::BattleUnit> units{};
    std::string error{};
    ASSERT_TRUE(game::battle::buildBattleUnitsFromCatalog(
        env.rpg_catalog,
        game::battle::BattleUnitBuildOptions{.troop_id = "troop.slime"},
        units,
        error)) << error;

    int slime_count = 0;
    for (auto& unit : units) {
        if (unit.side == game::battle::BattleSide::Enemy) {
            ++slime_count;
            unit.hp = 0;
            ASSERT_TRUE(unit.source_enemy_id.has_value());
            EXPECT_EQ(*unit.source_enemy_id, "enemy.slime");
        }
    }
    ASSERT_EQ(slime_count, 3);

    const game::domain::QuestBattleProgressResolver resolver{};
    const auto summary = resolver.apply(
        game::battle::BattleOutcome::Victory,
        units,
        env.quest_catalog,
        env.questLog());

    ASSERT_EQ(summary.updated_quests.size(), 1U);
    EXPECT_EQ(summary.updated_quests.front().quest_id, QUEST_ID);
    ASSERT_EQ(summary.became_ready_to_turn_in_quests.size(), 1U);
    EXPECT_EQ(summary.became_ready_to_turn_in_quests.front().quest_id, QUEST_ID);
}

} // namespace

namespace game::script {

TEST(ScriptQuestFlowTest, QuestModuleForFlattensQuestIds) {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    engine::script::ScriptHost host(registry);
    host.setScriptRoot(projectScriptRoot());
    ASSERT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));

    EXPECT_TRUE(host.exec(R"(
        local quest = tf.script.require("lib.quest")
        assert(quest.module_for("quest.village.goblin_cleanup") == "quests.village_goblin_cleanup")
        assert(quest.module_for("quest.first_delivery") == "quests.first_delivery")
        assert(quest.module_for("village.goblin_cleanup") == "quests.village_goblin_cleanup")
        assert(quest.module_for("") == nil)
        assert(quest.module_for("quest.") == nil)
        assert(quest.module_for("quest..bad") == nil)
        assert(quest.module_for("quest.bad/name") == nil)
        assert(quest.module_for("quest.bad-name") == nil)
    )"));
}

TEST(ScriptQuestFlowTest, ScriptedSlimeCleanupQuestRequestsOfferThenAcceptsProgressesAndTurnsIn) {
    ScriptQuestFlowEnv env{};
    QuestOfferCapture offers{};
    env.dispatcher.sink<game::defs::QuestOfferRequestedEvent>()
        .connect<&QuestOfferCapture::onRequest>(&offers);
    env.loadQuestScript();

    env.interact();
    env.interact();
    env.interact();

    auto& quest_log = env.questLog();
    ASSERT_EQ(offers.requests.size(), 1U);
    EXPECT_EQ(offers.requests.front().player, env.player);
    EXPECT_EQ(offers.requests.front().giver, env.giver);
    EXPECT_EQ(offers.requests.front().quest_id, QUEST_ID);
    EXPECT_TRUE(quest_log.active_quests.empty());
    EXPECT_TRUE(quest_log.objective_progress.empty());

    env.dispatcher.trigger(game::defs::AcceptQuestCommand{
        .player = offers.requests.front().player,
        .giver = offers.requests.front().giver,
        .quest_id_hash = offers.requests.front().quest_id_hash,
        .quest_id = offers.requests.front().quest_id,
    });

    ASSERT_EQ(quest_log.active_quests.size(), 1U);
    EXPECT_EQ(quest_log.active_quests.front(), QUEST_ID);
    EXPECT_EQ(
        quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, QUEST_OBJECTIVE_ID)],
        0);
    EXPECT_TRUE(env.host.exec(R"(
        assert(tf.state.get_bool("quest.village_goblin_cleanup.accepted", false) == true)
        assert(tf.state.get_int("quest.village_goblin_cleanup.accepted_count", 0) == 1)
    )"));

    defeatProjectSlimeTroop(env);
    EXPECT_EQ(
        quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, QUEST_OBJECTIVE_ID)],
        3);

    env.interact();
    env.interact();
    env.interact();

    EXPECT_TRUE(quest_log.active_quests.empty());
    ASSERT_EQ(quest_log.completed_quests.size(), 1U);
    EXPECT_EQ(quest_log.completed_quests.front(), QUEST_ID);
    EXPECT_EQ(env.registry.get<game::component::PlayerWalletComponent>(env.player).gold_, 50);
    const auto& inventory = env.registry.get<game::component::InventoryComponent>(env.player);
    EXPECT_EQ(inventory.slot(0).item_id_, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(inventory.slot(0).count_, 2);
    EXPECT_TRUE(env.host.exec(R"(
        assert(tf.state.get_bool("quest.village_goblin_cleanup.completed", false) == true)
        assert(tf.state.get_int("quest.village_goblin_cleanup.completed_count", 0) == 1)
    )"));

    env.interact();

    ASSERT_EQ(env.capture.shows.size(), 5U);
    EXPECT_EQ(env.capture.shows[0].text, "Can you help us drive away the slimes?");
    EXPECT_EQ(env.capture.shows[0].speaker, "Manu");
    EXPECT_EQ(env.capture.shows[0].speaker_actor_id, "npc.manu");
    EXPECT_EQ(env.capture.shows[1].text, "The old path is crawling with them. Three should be enough.");
    EXPECT_EQ(env.capture.shows[1].speaker, "Manu");
    EXPECT_EQ(env.capture.shows[2].text, "You did it? That's a relief.");
    EXPECT_EQ(env.capture.shows[2].speaker, "Manu");
    EXPECT_EQ(env.capture.shows[3].text, "Here, take this for the trouble.");
    EXPECT_EQ(env.capture.shows[3].speaker, "Manu");
    EXPECT_EQ(env.capture.shows[4].text, "Thank you again. The village path feels safe now.");
    EXPECT_EQ(env.capture.shows[4].speaker, "Manu");
    EXPECT_EQ(env.capture.hide_count, 2);
}

} // namespace game::script
