#include <gtest/gtest.h>

#include "engine/component/transform_component.h"
#include "engine/component/name_component.h"
#include "engine/script/script_host.h"
#include "game/component/actor_identity_component.h"
#include "game/component/inventory_component.h"
#include "game/component/npc_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/events_battle.h"
#include "game/defs/events_inventory.h"
#include "game/defs/events_quest.h"
#include "game/domain/inventory_domain_service.h"
#include "game/script/script_event_bridge.h"
#include "game/system/inventory_system.h"
#include "game/world/world_state.h"
#include "script_test_utils.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string itemConfigPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/data/item_use_items.json").string();
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
    engine::script::ScriptHost host;
    game::script::ScriptEventBridge bridge;
    entt::entity player{entt::null};
    entt::entity target{entt::null};

    ScriptEventBridgeTestEnv()
        : inventory_domain_service(registry, dispatcher, catalog),
          inventory_system(registry, dispatcher, catalog, inventory_domain_service),
          host(registry),
          bridge(host, registry, dispatcher) {
        EXPECT_TRUE(catalog.loadItemConfig(itemConfigPath()));
        player = registry.create();
        registry.emplace<game::component::PlayerTag>(player);
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0f, 0.0f});
        registry.emplace<game::component::InventoryComponent>(player);

        target = registry.create();
        registry.emplace<engine::component::TransformComponent>(target, glm::vec2{16.0f, 0.0f});

        EXPECT_TRUE(host.init(dispatcher, game::script::test::tinyFarmInstallers()));
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

    ASSERT_TRUE(env.host.exec(R"(
        seen_interact_metadata = false
        assert(tf.event.on("interact", function(evt)
            assert(evt.name == "interact")
            assert(evt.target ~= nil)
            assert(evt.target_actor_id == "actor.lyria")
            assert(evt.target_name == "Lyria")
            assert(evt.target_kind == "recruitable")
            assert(evt.target_blueprint_id == "lyria")
            assert(evt.map_id == "home_exterior")
            assert(type(evt.map_id_hash) == "string")
            assert(tf.entity.actor_id(evt.target) == "actor.lyria")
            assert(tf.entity.name(evt.target) == "Lyria")
            local x, y = tf.entity.position(evt.target)
            assert(x == 16.0)
            assert(y == 0.0)
            assert(tf.entity.has_component(evt.target, "actor_identity") == true)
            assert(tf.entity.has_component(evt.target, "recruitable") == true)
            assert(tf.entity.has_component(evt.target, "merchant") == false)
            seen_interact_metadata = true
        end) == true)
    )"));

    env.dispatcher.trigger(game::defs::InteractCommand{env.player, env.target});

    EXPECT_TRUE(env.host.exec("assert(seen_interact_metadata == true)"));
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

} // namespace game::script
