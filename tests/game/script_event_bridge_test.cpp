#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "engine/component/name_component.h"
#include "engine/script/script_host.h"
#include "game/component/actor_identity_component.h"
#include "game/component/chest_component.h"
#include "game/component/inventory_component.h"
#include "game/component/npc_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/script_trigger_component.h"
#include "game/component/script_zone_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/events_battle.h"
#include "game/defs/events_dialogue.h"
#include "game/defs/events_inventory.h"
#include "game/defs/events_map.h"
#include "game/defs/events_quest.h"
#include "game/domain/inventory_domain_service.h"
#include "game/script/script_event_bridge.h"
#include "game/system/chest_system.h"
#include "game/system/inventory_system.h"
#include "game/world/world_state.h"
#include "engine/utils/events.h"
#include "script_test_utils.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <optional>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string itemConfigPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/data/item_use_items.json").string();
}

[[nodiscard]] std::string projectScriptRoot() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "scripts").string();
}

[[nodiscard]] int countItem(const entt::registry& registry, const entt::entity player, const entt::id_type item_id) {
    const auto* inventory = registry.try_get<game::component::InventoryComponent>(player);
    if (!inventory) {
        return 0;
    }

    int total = 0;
    for (int slot_index = 0; slot_index < inventory->slotCount(); ++slot_index) {
        const auto& slot = inventory->slot(slot_index);
        if (slot.item_id_ == item_id) {
            total += slot.count_;
        }
    }
    return total;
}

struct ScriptEventBridgeTestEnv {
    entt::registry registry{};
    entt::dispatcher dispatcher{};
    game::data::ItemCatalog catalog{};
    game::domain::InventoryDomainService inventory_domain_service;
    game::system::InventorySystem inventory_system;
    game::world::WorldState world_state{};
    game::system::ChestSystem chest_system;
    engine::script::ScriptHost host;
    game::script::ScriptEventBridge bridge;
    entt::entity player{entt::null};
    entt::entity target{entt::null};

    ScriptEventBridgeTestEnv()
        : inventory_domain_service(registry, dispatcher, catalog),
          inventory_system(registry, dispatcher, inventory_domain_service),
          chest_system(registry, dispatcher, world_state, catalog, inventory_domain_service),
          host(registry),
          bridge(host, registry, dispatcher) {
        EXPECT_TRUE(catalog.loadItemConfig(itemConfigPath()));
        player = registry.create();
        registry.emplace<game::component::PlayerTag>(player);
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0f, 0.0f});
        registry.emplace<game::component::InventoryComponent>(player);

        target = registry.create();
        registry.emplace<engine::component::TransformComponent>(target, glm::vec2{16.0f, 0.0f});

        host.setScriptRoot(projectScriptRoot());
        EXPECT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));
    }
};

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};
    std::vector<game::defs::DialogueHideEvent> hides{};

    void onShow(const game::defs::DialogueShowEvent& event) {
        shows.push_back(event);
    }

    void onHide(const game::defs::DialogueHideEvent& event) {
        hides.push_back(event);
    }
};

struct AnimationCapture {
    std::vector<engine::utils::PlayAnimationEvent> plays{};

    void onPlay(const engine::utils::PlayAnimationEvent& event) {
        plays.push_back(event);
    }
};

} // namespace

namespace game::script {

TEST(ScriptEventBridgeTest, EventCallbackQueuesCommandsUntilDrain) {
    ScriptEventBridgeTestEnv env{};
    constexpr entt::id_type seed_item_id = entt::hashed_string{"strawberry_seed"}.value();

    ASSERT_TRUE(env.host.exec(R"(
        seen_interact = false
        assert(tf.event.on("interact", function(evt)
            assert(evt.name == "interact")
            assert(evt.player ~= nil)
            assert(evt.target ~= nil)
            seen_interact = true
            assert(tf.command.add_item("strawberry_seed", 2) == true)
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::InteractCommand{env.player, env.target});
    ASSERT_TRUE(env.host.exec("assert(seen_interact == true)"));
    EXPECT_EQ(countItem(env.registry, env.player, seed_item_id), 0);

    env.bridge.drainDeferredCommands();
    EXPECT_EQ(countItem(env.registry, env.player, seed_item_id), 2);
}

TEST(ScriptEventBridgeTest, FailedCallbackDiscardsQueuedCommands) {
    ScriptEventBridgeTestEnv env{};
    constexpr entt::id_type seed_item_id = entt::hashed_string{"strawberry_seed"}.value();

    ASSERT_TRUE(env.host.exec(R"(
        assert(tf.callbacks.on_interact(function()
            assert(tf.command.add_item("strawberry_seed", 1) == true)
            error("intentional callback failure")
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::InteractCommand{env.player, env.target});
    env.bridge.drainDeferredCommands();

    EXPECT_EQ(countItem(env.registry, env.player, seed_item_id), 0);
}

TEST(ScriptEventBridgeTest, InfiniteLoopCallbackIsAbortedAndLaterCallbacksStillRun) {
    ScriptEventBridgeTestEnv env{};
    constexpr entt::id_type seed_item_id = entt::hashed_string{"strawberry_seed"}.value();

    ASSERT_TRUE(env.host.exec(R"(
        assert(tf.callbacks.on_interact(function()
            while true do
            end
        end) == true)
        assert(tf.callbacks.on_interact(function()
            assert(tf.command.add_item("strawberry_seed", 1) == true)
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::InteractCommand{env.player, env.target});
    env.bridge.drainDeferredCommands();

    EXPECT_EQ(countItem(env.registry, env.player, seed_item_id), 1);
}

TEST(ScriptEventBridgeTest, LargeHashedIdsAreExposedAsStrings) {
    ScriptEventBridgeTestEnv env{};
    constexpr entt::id_type large_item_id = entt::hashed_string{"strawberry_seed"}.value();
    constexpr entt::id_type large_quest_id = entt::hashed_string{"quest.first_delivery"}.value();

    ASSERT_TRUE(env.host.exec(R"(
        inventory_item_id_type = nil
        item_used_id_type = nil
        quest_hash_type = nil
        assert(tf.event.on("inventory_changed", function(evt)
            inventory_item_id_type = type(evt.slots[1].item_id)
        end) == true)
        assert(tf.event.on("item_used", function(evt)
            item_used_id_type = type(evt.item_id)
        end) == true)
        assert(tf.event.on("quest_accepted", function(evt)
            quest_hash_type = type(evt.quest_id_hash)
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::InventoryChanged{
        .target = env.player,
        .slots = {game::defs::InventorySlotUpdate{
            .slot_index = 0,
            .item_id = large_item_id,
            .count = 1,
        }},
        .full_sync = false,
        .from_add = true,
    });
    env.dispatcher.trigger(game::defs::ItemUsedEvent{
        .target = env.player,
        .item_id = large_item_id,
        .inventory_slot_index = 0,
        .count = 1,
    });
    env.dispatcher.trigger(game::defs::QuestAcceptedEvent{
        .player = env.player,
        .giver = env.target,
        .quest_id_hash = large_quest_id,
        .quest_id = "quest.first_delivery",
    });

    EXPECT_TRUE(env.host.exec(R"(
        assert(inventory_item_id_type == "string")
        assert(item_used_id_type == "string")
        assert(quest_hash_type == "string")
    )"));
}

TEST(ScriptEventBridgeTest, InteractPayloadIncludesStableTargetMetadata) {
    ScriptEventBridgeTestEnv env{};
    game::world::WorldState world_state{};
    const entt::id_type map_id = world_state.ensureExternalMap("home_exterior");
    world_state.setCurrentMap(map_id);
    env.registry.ctx().emplace<game::world::WorldState*>(&world_state);

    env.registry.emplace<engine::component::NameComponent>(
        env.target,
        entt::hashed_string{"Lyria"}.value(),
        "Lyria");
    env.registry.emplace<game::component::NPCTag>(env.target);
    env.registry.emplace<game::component::ActorIdentityComponent>(
        env.target,
        game::component::ActorIdentityComponent{
            .actor_id_ = "actor.lyria",
            .actor_id_hash_ = entt::hashed_string{"actor.lyria"}.value(),
            .blueprint_id_ = "lyria",
        });
    env.registry.emplace<game::component::RecruitableComponent>(
        env.target,
        game::component::RecruitableComponent{
            .actor_id_ = "actor.lyria",
            .actor_id_hash_ = entt::hashed_string{"actor.lyria"}.value(),
        });
    env.registry.emplace<game::component::ScriptTriggerComponent>(
        env.target,
        game::component::ScriptTriggerComponent{
            .module_ = "npcs.lyria",
            .event_ = "lyria.intro",
            .once_key_ = "npc.lyria.intro",
        });

    ASSERT_TRUE(env.host.exec(R"(
        seen_interact_metadata = false
        assert(tf.event.on("interact", function(evt)
            assert(evt.name == "interact")
            assert(evt.target ~= nil)
            assert(evt.target_actor_id == "actor.lyria")
            assert(evt.target_name == "Lyria")
            assert(evt.target_kind == "recruitable")
            assert(evt.target_blueprint_id == "lyria")
            assert(evt.target_script_module == "npcs.lyria")
            assert(evt.target_script_event == "lyria.intro")
            assert(evt.target_script_once_key == "npc.lyria.intro")
            assert(evt.map_id == "home_exterior")
            assert(type(evt.map_id_hash) == "string")
            assert(tf.entity.actor_id(evt.target) == "actor.lyria")
            assert(tf.entity.name(evt.target) == "Lyria")
            local x, y = tf.entity.position(evt.target)
            assert(x == 16.0)
            assert(y == 0.0)
            assert(tf.entity.has_component(evt.target, "actor_identity") == true)
            assert(tf.entity.has_component(evt.target, "recruitable") == true)
            assert(tf.entity.has_component(evt.target, "script_trigger") == true)
            assert(tf.entity.has_component(evt.target, "merchant") == false)
            seen_interact_metadata = true
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::InteractCommand{env.player, env.target});

    EXPECT_TRUE(env.host.exec("assert(seen_interact_metadata == true)"));
}

TEST(ScriptEventBridgeTest, MapEnterAndExitEventsExposeStableMapIds) {
    ScriptEventBridgeTestEnv env{};
    constexpr entt::id_type home_id = entt::hashed_string{"home_exterior"}.value();
    constexpr entt::id_type town_id = entt::hashed_string{"town"}.value();

    ASSERT_TRUE(env.host.exec(R"(
        seen_map_exit = false
        seen_map_enter = false
        assert(tf.event.on("map_exit", function(evt)
            assert(evt.name == "map_exit")
            assert(evt.map_id == "home_exterior")
            assert(type(evt.map_id_hash) == "string")
            assert(evt.next_map_id == "town")
            assert(type(evt.next_map_id_hash) == "string")
            seen_map_exit = true
        end) == true)
        assert(tf.event.on("map_enter", function(evt)
            assert(evt.name == "map_enter")
            assert(evt.map_id == "town")
            assert(type(evt.map_id_hash) == "string")
            assert(evt.previous_map_id == "home_exterior")
            assert(type(evt.previous_map_id_hash) == "string")
            seen_map_enter = true
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::MapExitedEvent{
        .map_id = home_id,
        .map_name = "home_exterior",
        .next_map_id = town_id,
        .next_map_name = "town",
    });
    env.dispatcher.trigger(game::defs::MapEnteredEvent{
        .map_id = town_id,
        .map_name = "town",
        .previous_map_id = home_id,
        .previous_map_name = "home_exterior",
    });

    EXPECT_TRUE(env.host.exec(R"(
        assert(seen_map_exit == true)
        assert(seen_map_enter == true)
    )"));
}

TEST(ScriptEventBridgeTest, ZoneEnterAndExitEventsExposeStableZoneMetadata) {
    ScriptEventBridgeTestEnv env{};
    constexpr entt::id_type map_id = entt::hashed_string{"home_exterior"}.value();
    constexpr entt::id_type zone_hash = entt::hashed_string{"zone.home.seed_hint"}.value();

    const entt::entity zone = env.registry.create();
    env.registry.emplace<game::component::ScriptZoneComponent>(
        zone,
        game::component::ScriptZoneComponent{
            .rect_ = engine::utils::Rect{glm::vec2{10.0F, 10.0F}, glm::vec2{20.0F, 20.0F}},
            .map_id_ = map_id,
            .zone_id_ = "zone.home.seed_hint",
            .zone_id_hash_ = zone_hash,
        });
    env.registry.emplace<game::component::ScriptTriggerComponent>(
        zone,
        game::component::ScriptTriggerComponent{
            .module_ = "maps.home_exterior",
            .event_ = "home_exterior.seed_hint",
            .once_key_ = "map.home_exterior.seed_hint",
        });

    ASSERT_TRUE(env.host.exec(R"(
        seen_zone_enter = false
        seen_zone_exit = false
        assert(tf.event.on("zone_enter", function(evt)
            assert(evt.name == "zone_enter")
            assert(evt.player ~= nil)
            assert(evt.zone ~= nil)
            assert(evt.map_id == "home_exterior")
            assert(type(evt.map_id_hash) == "string")
            assert(evt.zone_id == "zone.home.seed_hint")
            assert(type(evt.zone_id_hash) == "string")
            assert(evt.zone_script_module == "maps.home_exterior")
            assert(evt.zone_script_event == "home_exterior.seed_hint")
            assert(evt.zone_script_once_key == "map.home_exterior.seed_hint")
            assert(tf.entity.has_component(evt.zone, "script_zone") == true)
            seen_zone_enter = true
        end) == true)
        assert(tf.event.on("zone_exit", function(evt)
            assert(evt.name == "zone_exit")
            assert(evt.zone_id == "zone.home.seed_hint")
            assert(evt.zone_script_event == "home_exterior.seed_hint")
            seen_zone_exit = true
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::ZoneEnteredEvent{
        .player = env.player,
        .zone = zone,
        .map_id = map_id,
        .map_name = "home_exterior",
        .zone_id = "zone.home.seed_hint",
        .zone_id_hash = zone_hash,
    });
    env.dispatcher.trigger(game::defs::ZoneExitedEvent{
        .player = env.player,
        .zone = zone,
        .map_id = map_id,
        .map_name = "home_exterior",
        .zone_id = "zone.home.seed_hint",
        .zone_id_hash = zone_hash,
    });

    EXPECT_TRUE(env.host.exec(R"(
        assert(seen_zone_enter == true)
        assert(seen_zone_exit == true)
    )"));
}

TEST(ScriptEventBridgeTest, HomeExteriorScriptHandlesFirstEnterAndScriptedSeedCacheOnce) {
    ScriptEventBridgeTestEnv env{};
    DialogueCapture dialogue_capture{};
    AnimationCapture animation_capture{};
    env.dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&dialogue_capture);
    env.dispatcher.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&dialogue_capture);
    env.dispatcher.sink<engine::utils::PlayAnimationEvent>().connect<&AnimationCapture::onPlay>(&animation_capture);

    const entt::id_type map_id = env.world_state.ensureExternalMap("home_exterior");
    env.world_state.setCurrentMap(map_id);
    env.registry.ctx().emplace<game::world::WorldState*>(&env.world_state);

    const entt::entity chest = env.registry.create();
    env.registry.emplace<engine::component::TransformComponent>(chest, glm::vec2{32.0F, 16.0F});
    env.registry.emplace<game::component::MapId>(chest, map_id);
    env.registry.emplace<game::component::ChestComponent>(
        chest,
        game::component::ChestComponent{.chest_id_ = 42});
    env.registry.emplace<game::component::ScriptedInteractionComponent>(chest);
    env.registry.emplace<game::component::ScriptTriggerComponent>(
        chest,
        game::component::ScriptTriggerComponent{
            .event_ = "home_exterior.seed_cache",
            .once_key_ = "map.home_exterior.seed_cache.opened",
        });

    ASSERT_TRUE(env.host.exec(R"(
        assert(tf.script.require("maps.home_exterior") ~= nil)
        assert(tf.state.get_bool("map.home_exterior.first_enter", false) == true)
    )"));
    env.bridge.drainDeferredCommands();
    env.dispatcher.update();
    ASSERT_EQ(dialogue_capture.shows.size(), 1U);
    EXPECT_EQ(dialogue_capture.shows.back().text,
              "The farm road is quiet today. Check the seed cache before heading out.");

    env.dispatcher.trigger(game::defs::MapEnteredEvent{
        .map_id = map_id,
        .map_name = "home_exterior",
    });
    env.bridge.drainDeferredCommands();
    env.dispatcher.update();
    EXPECT_EQ(dialogue_capture.shows.size(), 1U);

    env.dispatcher.trigger(game::defs::InteractCommand{env.player, chest});
    env.bridge.drainDeferredCommands();
    env.dispatcher.update();

    constexpr entt::id_type potato_seed = entt::hashed_string{"potato_seed"}.value();
    constexpr entt::id_type strawberry_seed = entt::hashed_string{"strawberry_seed"}.value();
    EXPECT_EQ(countItem(env.registry, env.player, potato_seed), 2);
    EXPECT_EQ(countItem(env.registry, env.player, strawberry_seed), 3);
    ASSERT_EQ(dialogue_capture.shows.size(), 2U);
    EXPECT_EQ(dialogue_capture.shows.back().text, "You found a few starter seeds.");
    EXPECT_EQ(dialogue_capture.shows.back().channel, game::defs::DialogueChannel::Notice);
    ASSERT_EQ(animation_capture.plays.size(), 1U);
    EXPECT_EQ(animation_capture.plays.back().entity_, chest);
    EXPECT_EQ(animation_capture.plays.back().animation_id_, entt::hashed_string{"open"}.value());
    EXPECT_FALSE(animation_capture.plays.back().loop_);
    EXPECT_TRUE(env.registry.get<game::component::ChestComponent>(chest).opened_);
    ASSERT_NE(env.world_state.getMapState(map_id), nullptr);
    EXPECT_TRUE(env.world_state.getMapState(map_id)->persistent.opened_chests.contains(42));

    env.chest_system.update(2.0F);
    env.dispatcher.update();
    ASSERT_FALSE(dialogue_capture.hides.empty());
    EXPECT_EQ(dialogue_capture.hides.back().channel, game::defs::DialogueChannel::Notice);

    env.dispatcher.trigger(game::defs::InteractCommand{env.player, chest});
    env.bridge.drainDeferredCommands();
    env.dispatcher.update();

    EXPECT_EQ(countItem(env.registry, env.player, potato_seed), 2);
    EXPECT_EQ(countItem(env.registry, env.player, strawberry_seed), 3);
    ASSERT_EQ(dialogue_capture.shows.size(), 3U);
    EXPECT_EQ(dialogue_capture.shows.back().text, "The seed cache is empty.");
    EXPECT_TRUE(env.host.exec(R"(
        assert(tf.state.get_bool("map.home_exterior.seed_cache.opened", false) == true)
    )"));

    env.dispatcher.disconnect(&dialogue_capture);
    env.dispatcher.disconnect(&animation_capture);
}

TEST(ScriptEventBridgeTest, BattleEndedPayloadIncludesRewardSummary) {
    ScriptEventBridgeTestEnv env{};
    constexpr entt::id_type seed_item_id = entt::hashed_string{"strawberry_seed"}.value();

    ASSERT_TRUE(env.host.exec(R"(
        battle_reward_seen = false
        assert(tf.callbacks.on_battle_end(function(evt)
            assert(evt.name == "battle_ended")
            assert(evt.outcome == "Victory")
            assert(evt.has_rewards == true)
            assert(evt.rewards.gold == 7)
            assert(evt.rewards.exp == 11)
            assert(#evt.rewards.items == 1)
            assert(evt.rewards.items[1].item_id == "strawberry_seed")
            assert(type(evt.rewards.items[1].item_id_hash) == "string")
            assert(evt.rewards.items[1].count == 2)
            battle_reward_seen = true
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::BattleEndedEvent{
        .outcome = game::battle::BattleOutcome::Victory,
        .reward_summary = game::battle::BattleRewardSummary{
            .gold_total = 7,
            .exp_total = 11,
            .item_drops = {game::battle::BattleRewardItemDrop{
                .item_id = "strawberry_seed",
                .item_id_hash = seed_item_id,
                .count = 2,
            }},
        },
    });

    EXPECT_TRUE(env.host.exec("assert(battle_reward_seen == true)"));
}

TEST(ScriptEventBridgeTest, BattleHookPayloadsExposeUnitsAndActionResult) {
    ScriptEventBridgeTestEnv env{};

    game::battle::BattleUnit hero{};
    hero.id = 1;
    hero.name = "Hero";
    hero.side = game::battle::BattleSide::Player;
    hero.hp = 24;
    hero.max_hp = 30;
    hero.mp = 8;
    hero.max_mp = 12;
    hero.source_actor_id = std::optional<std::string>{"actor.hero"};

    game::battle::BattleUnit hero_after = hero;
    hero_after.mp = 4;

    game::battle::BattleUnit slime{};
    slime.id = 2;
    slime.name = "Slime";
    slime.side = game::battle::BattleSide::Enemy;
    slime.hp = 9;
    slime.max_hp = 9;
    slime.source_enemy_id = std::optional<std::string>{"enemy.slime"};

    game::battle::BattleUnit slime_after = slime;
    slime_after.hp = 0;

    game::battle::BattleActionResult result{};
    result.status = game::battle::BattleActionStatus::Applied;
    result.action_type = game::battle::BattleActionType::Skill;
    result.actor_id = hero.id;
    result.target_id = slime.id;
    result.skill_id = "skill.fire";
    result.damage = 12;
    result.mp_spent = 4;
    result.target_defeated = true;
    result.states_added = {"state.burn"};
    result.outcome_after = game::battle::BattleOutcome::Victory;
    result.snapshot.units = {hero_after, slime_after};
    result.snapshot.round_index = 2;
    result.snapshot.outcome = game::battle::BattleOutcome::Victory;

    ASSERT_TRUE(env.host.exec(R"(
        battle_turn_started_seen = false
        battle_turn_ended_seen = false
        battle_unit_died_seen = false
        battle_skill_used_seen = false

        assert(tf.battle.on_turn_start(function(evt)
            assert(evt.name == "battle_turn_started")
            assert(evt.round_index == 2)
            assert(evt.unit_id == 1)
            assert(evt.actor_id == "actor.hero")
            assert(evt.unit.unit_kind == "actor")
            assert(evt.unit.hp == 24)
            battle_turn_started_seen = true
        end) == true)

        assert(tf.battle.on_turn_end(function(evt)
            assert(evt.name == "battle_turn_ended")
            assert(evt.round_index == 2)
            assert(evt.unit.mp == 4)
            assert(evt.result.status == "Applied")
            assert(evt.result.action_type == "Skill")
            assert(evt.result.target_unit_id == 2)
            assert(evt.result.skill_id == "skill.fire")
            assert(evt.result.damage == 12)
            assert(evt.result.mp_spent == 4)
            assert(evt.result.target_defeated == true)
            assert(evt.result.outcome_after == "Victory")
            assert(evt.result.states_added[1] == "state.burn")
            battle_turn_ended_seen = true
        end) == true)

        assert(tf.battle.on_unit_died(function(evt)
            assert(evt.name == "battle_unit_died")
            assert(evt.unit_id == 2)
            assert(evt.unit_kind == "enemy")
            assert(evt.actor_id == nil)
            assert(evt.enemy_id == "enemy.slime")
            assert(evt.unit.alive == false)
            assert(evt.source_unit_id == 1)
            assert(evt.source_action_type == "Skill")
            assert(evt.skill_id == "skill.fire")
            battle_unit_died_seen = true
        end) == true)

        assert(tf.battle.on_skill_used(function(evt)
            assert(evt.name == "battle_skill_used")
            assert(evt.unit_id == 1)
            assert(evt.actor_id == "actor.hero")
            assert(evt.skill_id == "skill.fire")
            assert(evt.target_unit_id == 2)
            assert(evt.result.snapshot_round_index == 2)
            battle_skill_used_seen = true
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::BattleTurnStartedEvent{
        .unit = hero,
        .round_index = 2,
    });
    env.dispatcher.trigger(game::defs::BattleTurnEndedEvent{
        .unit = hero_after,
        .result = result,
        .round_index = 2,
    });
    env.dispatcher.trigger(game::defs::BattleUnitDiedEvent{
        .unit = slime_after,
        .source_unit_id = hero.id,
        .source_action_type = game::battle::BattleActionType::Skill,
        .skill_id = "skill.fire",
        .round_index = 2,
    });
    env.dispatcher.trigger(game::defs::BattleSkillUsedEvent{
        .unit = hero_after,
        .result = result,
        .round_index = 2,
    });

    EXPECT_TRUE(env.host.exec(R"(
        assert(battle_turn_started_seen == true)
        assert(battle_turn_ended_seen == true)
        assert(battle_unit_died_seen == true)
        assert(battle_skill_used_seen == true)
    )"));
}

} // namespace game::script
