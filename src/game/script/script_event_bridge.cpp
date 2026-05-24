#include "script_event_bridge.h"

#include "engine/script/script_entity_handle.h"
#include "engine/script/script_host.h"
#include "engine/utils/events.h"
#include "engine/component/name_component.h"
#include "game/battle/battle_types.h"
#include "game/component/actor_identity_component.h"
#include "game/component/chest_component.h"
#include "game/component/merchant_component.h"
#include "game/component/npc_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/recruitable_component.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/events_battle.h"
#include "game/defs/events_dialogue.h"
#include "game/defs/events_inventory.h"
#include "game/defs/events_quest.h"
#include "game/world/world_state.h"

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

void setOptionalString(sol::table& payload, const char* key, const std::string& value) {
    if (value.empty()) {
        payload[key] = sol::lua_nil;
        return;
    }
    payload[key] = value;
}

[[nodiscard]] std::string_view targetKind(const entt::registry& registry, const entt::entity target) {
    if (target == entt::null || !registry.valid(target)) {
        return "unknown";
    }
    if (registry.any_of<game::component::MerchantComponent>(target)) {
        return "merchant";
    }
    if (registry.any_of<game::component::QuestGiverComponent>(target)) {
        return "quest_giver";
    }
    if (registry.any_of<game::component::RecruitableComponent>(target)) {
        return "recruitable";
    }
    if (registry.any_of<game::component::ChestComponent>(target)) {
        return "chest";
    }
    if (registry.any_of<game::component::NPCTag, game::component::DialogueComponent>(target)) {
        return "npc";
    }
    return "unknown";
}

[[nodiscard]] entt::id_type currentMapId(entt::registry& registry) {
    auto** world_state_ptr = registry.ctx().find<game::world::WorldState*>();
    if (!world_state_ptr || !*world_state_ptr) {
        return entt::null;
    }
    return (*world_state_ptr)->getCurrentMap();
}

[[nodiscard]] std::string currentMapName(entt::registry& registry, const entt::id_type map_id) {
    if (map_id == entt::null) {
        return {};
    }

    auto** world_state_ptr = registry.ctx().find<game::world::WorldState*>();
    if (!world_state_ptr || !*world_state_ptr) {
        return {};
    }

    const auto* map_state = (*world_state_ptr)->getMapState(map_id);
    return map_state ? map_state->info.name : std::string{};
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
    const bool target_valid = event.target != entt::null && registry_.valid(event.target);
    payload["target_kind"] = std::string{targetKind(registry_, event.target)};

    if (const auto* identity = target_valid
                                   ? registry_.try_get<game::component::ActorIdentityComponent>(event.target)
                                   : nullptr) {
        setOptionalString(payload, "target_actor_id", identity->actor_id_);
        if (identity->actor_id_hash_ != entt::null) {
            setIdHash(payload, "target_actor_id_hash", identity->actor_id_hash_);
        } else {
            payload["target_actor_id_hash"] = sol::lua_nil;
        }
        setOptionalString(payload, "target_blueprint_id", identity->blueprint_id_);
    } else {
        payload["target_actor_id"] = sol::lua_nil;
        payload["target_actor_id_hash"] = sol::lua_nil;
        payload["target_blueprint_id"] = sol::lua_nil;
    }

    if (const auto* name = target_valid ? registry_.try_get<engine::component::NameComponent>(event.target) : nullptr) {
        setOptionalString(payload, "target_name", name->name_);
    } else {
        payload["target_name"] = sol::lua_nil;
    }

    const entt::id_type map_id = currentMapId(registry_);
    setOptionalString(payload, "map_id", currentMapName(registry_, map_id));
    if (map_id != entt::null) {
        setIdHash(payload, "map_id_hash", map_id);
    } else {
        payload["map_id_hash"] = sol::lua_nil;
    }
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
    if (event.reward_summary.has_value()) {
        sol::table rewards = host_.luaState().create_table();
        rewards["gold"] = event.reward_summary->gold_total;
        rewards["exp"] = event.reward_summary->exp_total;

        sol::table items = host_.luaState().create_table();
        for (const auto& item : event.reward_summary->item_drops) {
            sol::table item_payload = host_.luaState().create_table();
            item_payload["item_id"] = item.item_id;
            item_payload["item_id_hash"] = std::to_string(item.item_id_hash);
            item_payload["count"] = item.count;
            items.add(item_payload);
        }
        rewards["items"] = items;
        payload["rewards"] = rewards;
    } else {
        payload["rewards"] = sol::lua_nil;
    }
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
