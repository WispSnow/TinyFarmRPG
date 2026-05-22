#include "script_event_bridge.h"

#include "engine/script/script_entity_handle.h"
#include "engine/script/script_host.h"
#include "engine/utils/events.h"
#include "game/battle/battle_types.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/events_battle.h"
#include "game/defs/events_dialogue.h"
#include "game/defs/events_inventory.h"
#include "game/defs/events_quest.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <sol/sol.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace game::script {

namespace {

[[nodiscard]] std::string_view timeOfDayName(const std::uint8_t value) {
    switch (value) {
        case 0:
            return "Dawn";
        case 1:
            return "Day";
        case 2:
            return "Dusk";
        case 3:
            return "Night";
        default:
            return "Unknown";
    }
}

[[nodiscard]] std::string_view dialogueChannelName(const game::defs::DialogueChannel channel) {
    switch (channel) {
        case game::defs::DialogueChannel::Conversation:
            return "conversation";
        case game::defs::DialogueChannel::Notice:
            return "notice";
        case game::defs::DialogueChannel::ItemNotice:
            return "item_notice";
    }

    return "unknown";
}

[[nodiscard]] std::string_view inventoryMoveKindName(const game::defs::InventoryMoveKind kind) {
    switch (kind) {
        case game::defs::InventoryMoveKind::None:
            return "none";
        case game::defs::InventoryMoveKind::MoveToEmpty:
            return "move_to_empty";
        case game::defs::InventoryMoveKind::Swap:
            return "swap";
        case game::defs::InventoryMoveKind::Merge:
            return "merge";
    }

    return "unknown";
}

void setEntityHandle(engine::script::ScriptHost& host,
                     const entt::registry& registry,
                     sol::table& payload,
                     const char* key,
                     const entt::entity entity) {
    if (entity == entt::null || !registry.valid(entity)) {
        payload[key] = sol::lua_nil;
        return;
    }

    payload[key] = host.makeHandle(entity);
}

void setEventName(sol::table& payload, const std::string_view event_name) {
    payload["name"] = std::string{event_name};
}

void setIdHash(sol::table& payload, const char* key, const entt::id_type value) {
    payload[key] = std::to_string(value);
}

} // namespace

ScriptEventBridge::ScriptEventBridge(engine::script::ScriptHost& host,
                                     entt::registry& registry,
                                     entt::dispatcher& dispatcher)
    : host_(host),
      registry_(registry),
      dispatcher_(dispatcher) {
    subscribe();
}

ScriptEventBridge::~ScriptEventBridge() {
    unsubscribe();
}

void ScriptEventBridge::drainDeferredCommands() {
    host_.drainDeferredCommands();
}

void ScriptEventBridge::subscribe() {
    dispatcher_.sink<game::defs::InteractCommand>().connect<&ScriptEventBridge::onInteract>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&ScriptEventBridge::onDialogueClosed>(this);
    dispatcher_.sink<game::defs::InventoryChanged>().connect<&ScriptEventBridge::onInventoryChanged>(this);
    dispatcher_.sink<game::defs::ItemUsedEvent>().connect<&ScriptEventBridge::onItemUsed>(this);
    dispatcher_.sink<game::defs::BattleStartedEvent>().connect<&ScriptEventBridge::onBattleStarted>(this);
    dispatcher_.sink<game::defs::BattleEndedEvent>().connect<&ScriptEventBridge::onBattleEnded>(this);
    dispatcher_.sink<engine::utils::DayChangedEvent>().connect<&ScriptEventBridge::onDayChanged>(this);
    dispatcher_.sink<engine::utils::TimeOfDayChangedEvent>().connect<&ScriptEventBridge::onTimeOfDayChanged>(this);
    dispatcher_.sink<game::defs::QuestAcceptedEvent>().connect<&ScriptEventBridge::onQuestAccepted>(this);
    dispatcher_.sink<game::defs::QuestCompletedEvent>().connect<&ScriptEventBridge::onQuestCompleted>(this);
}

void ScriptEventBridge::unsubscribe() {
    dispatcher_.sink<game::defs::InteractCommand>().disconnect<&ScriptEventBridge::onInteract>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().disconnect<&ScriptEventBridge::onDialogueClosed>(this);
    dispatcher_.sink<game::defs::InventoryChanged>().disconnect<&ScriptEventBridge::onInventoryChanged>(this);
    dispatcher_.sink<game::defs::ItemUsedEvent>().disconnect<&ScriptEventBridge::onItemUsed>(this);
    dispatcher_.sink<game::defs::BattleStartedEvent>().disconnect<&ScriptEventBridge::onBattleStarted>(this);
    dispatcher_.sink<game::defs::BattleEndedEvent>().disconnect<&ScriptEventBridge::onBattleEnded>(this);
    dispatcher_.sink<engine::utils::DayChangedEvent>().disconnect<&ScriptEventBridge::onDayChanged>(this);
    dispatcher_.sink<engine::utils::TimeOfDayChangedEvent>().disconnect<&ScriptEventBridge::onTimeOfDayChanged>(this);
    dispatcher_.sink<game::defs::QuestAcceptedEvent>().disconnect<&ScriptEventBridge::onQuestAccepted>(this);
    dispatcher_.sink<game::defs::QuestCompletedEvent>().disconnect<&ScriptEventBridge::onQuestCompleted>(this);
}

void ScriptEventBridge::onInteract(const game::defs::InteractCommand& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "interact");
    setEntityHandle(host_, registry_, payload, "player", event.player);
    setEntityHandle(host_, registry_, payload, "target", event.target);
    (void)host_.emitEvent("interact", payload);
}

void ScriptEventBridge::onDialogueClosed(const game::defs::DialogueHideEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "dialogue_closed");
    setEntityHandle(host_, registry_, payload, "target", event.target);
    payload["channel"] = static_cast<int>(event.channel);
    payload["channel_name"] = std::string{dialogueChannelName(event.channel)};
    (void)host_.emitEvent("dialogue_closed", payload);
}

void ScriptEventBridge::onInventoryChanged(const game::defs::InventoryChanged& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "inventory_changed");
    setEntityHandle(host_, registry_, payload, "target", event.target);
    payload["full_sync"] = event.full_sync;
    payload["from_add"] = event.from_add;
    payload["move_kind"] = std::string{inventoryMoveKindName(event.move_kind)};
    payload["move_from_slot"] = event.move_from_slot;
    payload["move_to_slot"] = event.move_to_slot;

    sol::table slots = host_.luaState().create_table();
    for (const auto& slot : event.slots) {
        sol::table slot_payload = host_.luaState().create_table();
        slot_payload["slot_index"] = slot.slot_index;
        setIdHash(slot_payload, "item_id", slot.item_id);
        setIdHash(slot_payload, "item_id_hash", slot.item_id);
        slot_payload["count"] = slot.count;
        slots.add(slot_payload);
    }
    payload["slots"] = slots;
    (void)host_.emitEvent("inventory_changed", payload);
}

void ScriptEventBridge::onItemUsed(const game::defs::ItemUsedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "item_used");
    setEntityHandle(host_, registry_, payload, "target", event.target);
    setIdHash(payload, "item_id", event.item_id);
    setIdHash(payload, "item_id_hash", event.item_id);
    payload["inventory_slot_index"] = event.inventory_slot_index;
    payload["count"] = event.count;
    if (event.actor_target_id.has_value()) {
        payload["actor_target_id"] = *event.actor_target_id;
    } else {
        payload["actor_target_id"] = sol::lua_nil;
    }
    (void)host_.emitEvent("item_used", payload);
}

void ScriptEventBridge::onBattleStarted(const game::defs::BattleStartedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "battle_started");
    payload["troop_id"] = event.troop_id;
    payload["battle_background_id"] = event.battle_background_id;
    payload["from_encounter"] = event.from_encounter;
    payload["encounter_id"] = event.encounter_id;

    sol::table actors = host_.luaState().create_table();
    for (const auto& actor_id : event.actor_ids) {
        actors.add(actor_id);
    }
    payload["actor_ids"] = actors;
    (void)host_.emitEvent("battle_started", payload);
}

void ScriptEventBridge::onBattleEnded(const game::defs::BattleEndedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "battle_ended");
    payload["outcome"] = std::string{game::battle::toString(event.outcome)};
    payload["final_unit_count"] = static_cast<int>(event.final_units.size());
    payload["has_rewards"] = event.reward_summary.has_value();
    (void)host_.emitEvent("battle_ended", payload);
}

void ScriptEventBridge::onDayChanged(const engine::utils::DayChangedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "day_changed");
    payload["day"] = event.day_;
    (void)host_.emitEvent("day_changed", payload);
}

void ScriptEventBridge::onTimeOfDayChanged(const engine::utils::TimeOfDayChangedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "time_of_day_changed");
    payload["day"] = event.day_;
    payload["hour"] = event.hour_;
    payload["time_of_day"] = static_cast<int>(event.time_of_day_);
    payload["time_of_day_name"] = std::string{timeOfDayName(event.time_of_day_)};
    (void)host_.emitEvent("time_of_day_changed", payload);
}

void ScriptEventBridge::onQuestAccepted(const game::defs::QuestAcceptedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "quest_accepted");
    setEntityHandle(host_, registry_, payload, "player", event.player);
    setEntityHandle(host_, registry_, payload, "giver", event.giver);
    setIdHash(payload, "quest_id_hash", event.quest_id_hash);
    payload["quest_id"] = event.quest_id;
    (void)host_.emitEvent("quest_accepted", payload);
}

void ScriptEventBridge::onQuestCompleted(const game::defs::QuestCompletedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "quest_completed");
    setEntityHandle(host_, registry_, payload, "player", event.player);
    setEntityHandle(host_, registry_, payload, "giver", event.giver);
    setIdHash(payload, "quest_id_hash", event.quest_id_hash);
    payload["quest_id"] = event.quest_id;
    (void)host_.emitEvent("quest_completed", payload);
}

} // namespace game::script
