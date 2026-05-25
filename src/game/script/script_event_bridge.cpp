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
#include "game/component/script_trigger_component.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/events_battle.h"
#include "game/defs/events_dialogue.h"
#include "game/defs/events_inventory.h"
#include "game/defs/events_map.h"
#include "game/defs/events_quest.h"
#include "game/world/world_state.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <sol/sol.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

[[nodiscard]] std::string_view battleActionTypeName(const game::battle::BattleActionType type) {
    switch (type) {
        case game::battle::BattleActionType::Attack:
            return "Attack";
        case game::battle::BattleActionType::Skill:
            return "Skill";
        case game::battle::BattleActionType::Item:
            return "Item";
        case game::battle::BattleActionType::Guard:
            return "Guard";
        case game::battle::BattleActionType::Escape:
            return "Escape";
        case game::battle::BattleActionType::EndTurn:
            return "EndTurn";
    }

    return "Unknown";
}

[[nodiscard]] std::string_view battleActionStatusName(const game::battle::BattleActionStatus status) {
    switch (status) {
        case game::battle::BattleActionStatus::Applied:
            return "Applied";
        case game::battle::BattleActionStatus::Rejected:
            return "Rejected";
    }

    return "Unknown";
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

void setOptionalString(sol::table& payload, const char* key, const std::optional<std::string>& value) {
    if (!value.has_value() || value->empty()) {
        payload[key] = sol::lua_nil;
        return;
    }
    payload[key] = *value;
}

void setOptionalUnitId(sol::table& payload,
                       const char* key,
                       const std::optional<game::battle::BattleUnitId>& value) {
    if (!value.has_value()) {
        payload[key] = sol::lua_nil;
        return;
    }
    payload[key] = static_cast<int>(*value);
}

[[nodiscard]] sol::table stringListToLua(sol::state& lua, const std::vector<std::string>& values) {
    sol::table payload = lua.create_table();
    for (const auto& value : values) {
        payload.add(value);
    }
    return payload;
}

[[nodiscard]] std::string_view battleUnitKindName(const game::battle::BattleUnit& unit) {
    if (unit.source_actor_id.has_value()) {
        return "actor";
    }
    if (unit.source_enemy_id.has_value()) {
        return "enemy";
    }
    // Synthetic or test units may not carry catalog ids; side keeps the payload usable while actor_id/enemy_id stay nil.
    return unit.side == game::battle::BattleSide::Player ? "player" : "enemy";
}

[[nodiscard]] sol::table battleUnitToLua(sol::state& lua, const game::battle::BattleUnit& unit) {
    sol::table payload = lua.create_table();
    payload["unit_id"] = static_cast<int>(unit.id);
    payload["name"] = unit.name;
    payload["side"] = std::string{game::battle::toString(unit.side)};
    payload["unit_kind"] = std::string{battleUnitKindName(unit)};
    setOptionalString(payload, "actor_id", unit.source_actor_id);
    setOptionalString(payload, "enemy_id", unit.source_enemy_id);
    payload["hp"] = unit.hp;
    payload["max_hp"] = unit.max_hp;
    payload["mp"] = unit.mp;
    payload["max_mp"] = unit.max_mp;
    payload["alive"] = unit.isAlive();
    return payload;
}

void populateBattleUnitFields(sol::table& payload,
                              sol::state& lua,
                              const game::battle::BattleUnit& unit,
                              const std::uint32_t round_index) {
    payload["round_index"] = round_index;
    payload["unit"] = battleUnitToLua(lua, unit);
    payload["unit_id"] = static_cast<int>(unit.id);
    setOptionalString(payload, "actor_id", unit.source_actor_id);
    setOptionalString(payload, "enemy_id", unit.source_enemy_id);
    payload["unit_kind"] = std::string{battleUnitKindName(unit)};
}

[[nodiscard]] sol::table battleActionResultToLua(sol::state& lua, const game::battle::BattleActionResult& result) {
    sol::table payload = lua.create_table();
    payload["status"] = std::string{battleActionStatusName(result.status)};
    payload["action_type"] = std::string{battleActionTypeName(result.action_type)};
    payload["actor_unit_id"] = static_cast<int>(result.actor_id);
    setOptionalUnitId(payload, "target_unit_id", result.target_id);
    setOptionalString(payload, "skill_id", result.skill_id);
    setOptionalString(payload, "item_id", result.item_id);
    payload["damage"] = result.damage;
    payload["hp_recovered"] = result.hp_recovered;
    payload["mp_recovered"] = result.mp_recovered;
    payload["mp_spent"] = result.mp_spent;
    payload["missed"] = result.missed;
    payload["critical"] = result.critical;
    payload["target_guarded"] = result.target_guarded;
    payload["target_defeated"] = result.target_defeated;
    payload["escape_succeeded"] = result.escape_succeeded;
    setOptionalString(payload, "failure_reason", result.failure_reason);
    payload["outcome_after"] = std::string{game::battle::toString(result.outcome_after)};
    payload["snapshot_round_index"] = result.snapshot.round_index;
    payload["states_added"] = stringListToLua(lua, result.states_added);
    payload["states_removed"] = stringListToLua(lua, result.states_removed);
    return payload;
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

void setMapIdentity(entt::registry& registry,
                    sol::table& payload,
                    const char* name_key,
                    const char* hash_key,
                    const entt::id_type map_id,
                    const std::string& map_name) {
    if (!map_name.empty()) {
        payload[name_key] = map_name;
    } else {
        setOptionalString(payload, name_key, currentMapName(registry, map_id));
    }
    if (map_id != entt::null) {
        setIdHash(payload, hash_key, map_id);
    } else {
        payload[hash_key] = sol::lua_nil;
    }
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
    dispatcher_.sink<game::defs::DialogueChoiceSelectedEvent>()
        .connect<&ScriptEventBridge::onDialogueChoiceSelected>(this);
    dispatcher_.sink<game::defs::InventoryChanged>().connect<&ScriptEventBridge::onInventoryChanged>(this);
    dispatcher_.sink<game::defs::ItemUsedEvent>().connect<&ScriptEventBridge::onItemUsed>(this);
    dispatcher_.sink<game::defs::BattleStartedEvent>().connect<&ScriptEventBridge::onBattleStarted>(this);
    dispatcher_.sink<game::defs::BattleTurnStartedEvent>().connect<&ScriptEventBridge::onBattleTurnStarted>(this);
    dispatcher_.sink<game::defs::BattleTurnEndedEvent>().connect<&ScriptEventBridge::onBattleTurnEnded>(this);
    dispatcher_.sink<game::defs::BattleUnitDiedEvent>().connect<&ScriptEventBridge::onBattleUnitDied>(this);
    dispatcher_.sink<game::defs::BattleSkillUsedEvent>().connect<&ScriptEventBridge::onBattleSkillUsed>(this);
    dispatcher_.sink<game::defs::BattleEndedEvent>().connect<&ScriptEventBridge::onBattleEnded>(this);
    dispatcher_.sink<game::defs::MapEnteredEvent>().connect<&ScriptEventBridge::onMapEntered>(this);
    dispatcher_.sink<game::defs::MapExitedEvent>().connect<&ScriptEventBridge::onMapExited>(this);
    dispatcher_.sink<engine::utils::DayChangedEvent>().connect<&ScriptEventBridge::onDayChanged>(this);
    dispatcher_.sink<engine::utils::TimeOfDayChangedEvent>().connect<&ScriptEventBridge::onTimeOfDayChanged>(this);
    dispatcher_.sink<game::defs::QuestAcceptedEvent>().connect<&ScriptEventBridge::onQuestAccepted>(this);
    dispatcher_.sink<game::defs::QuestCompletedEvent>().connect<&ScriptEventBridge::onQuestCompleted>(this);
}

void ScriptEventBridge::unsubscribe() {
    dispatcher_.sink<game::defs::InteractCommand>().disconnect<&ScriptEventBridge::onInteract>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().disconnect<&ScriptEventBridge::onDialogueClosed>(this);
    dispatcher_.sink<game::defs::DialogueChoiceSelectedEvent>()
        .disconnect<&ScriptEventBridge::onDialogueChoiceSelected>(this);
    dispatcher_.sink<game::defs::InventoryChanged>().disconnect<&ScriptEventBridge::onInventoryChanged>(this);
    dispatcher_.sink<game::defs::ItemUsedEvent>().disconnect<&ScriptEventBridge::onItemUsed>(this);
    dispatcher_.sink<game::defs::BattleStartedEvent>().disconnect<&ScriptEventBridge::onBattleStarted>(this);
    dispatcher_.sink<game::defs::BattleTurnStartedEvent>().disconnect<&ScriptEventBridge::onBattleTurnStarted>(this);
    dispatcher_.sink<game::defs::BattleTurnEndedEvent>().disconnect<&ScriptEventBridge::onBattleTurnEnded>(this);
    dispatcher_.sink<game::defs::BattleUnitDiedEvent>().disconnect<&ScriptEventBridge::onBattleUnitDied>(this);
    dispatcher_.sink<game::defs::BattleSkillUsedEvent>().disconnect<&ScriptEventBridge::onBattleSkillUsed>(this);
    dispatcher_.sink<game::defs::BattleEndedEvent>().disconnect<&ScriptEventBridge::onBattleEnded>(this);
    dispatcher_.sink<game::defs::MapEnteredEvent>().disconnect<&ScriptEventBridge::onMapEntered>(this);
    dispatcher_.sink<game::defs::MapExitedEvent>().disconnect<&ScriptEventBridge::onMapExited>(this);
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

    if (const auto* script = target_valid
                                 ? registry_.try_get<game::component::ScriptTriggerComponent>(event.target)
                                 : nullptr) {
        setOptionalString(payload, "target_script_module", script->module_);
        setOptionalString(payload, "target_script_event", script->event_);
        setOptionalString(payload, "target_script_once_key", script->once_key_);
    } else {
        payload["target_script_module"] = sol::lua_nil;
        payload["target_script_event"] = sol::lua_nil;
        payload["target_script_once_key"] = sol::lua_nil;
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

void ScriptEventBridge::onDialogueChoiceSelected(const game::defs::DialogueChoiceSelectedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "dialogue_choice_selected");
    setEntityHandle(host_, registry_, payload, "target", event.target);
    payload["request_id"] = event.request_id;
    payload["cancelled"] = event.cancelled;
    if (event.cancelled || event.option_index < 0) {
        payload["choice_index"] = sol::lua_nil;
        payload["choice_zero_index"] = sol::lua_nil;
        payload["choice_id"] = sol::lua_nil;
        payload["choice_label"] = sol::lua_nil;
    } else {
        payload["choice_index"] = event.option_index + 1;
        payload["choice_zero_index"] = event.option_index;
        setOptionalString(payload, "choice_id", event.choice_id);
        setOptionalString(payload, "choice_label", event.choice_label);
    }
    (void)host_.emitEvent("dialogue_choice_selected", payload);
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

void ScriptEventBridge::onBattleTurnStarted(const game::defs::BattleTurnStartedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "battle_turn_started");
    populateBattleUnitFields(payload, host_.luaState(), event.unit, event.round_index);
    (void)host_.emitEvent("battle_turn_started", payload);
}

void ScriptEventBridge::onBattleTurnEnded(const game::defs::BattleTurnEndedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "battle_turn_ended");
    populateBattleUnitFields(payload, host_.luaState(), event.unit, event.round_index);
    payload["result"] = battleActionResultToLua(host_.luaState(), event.result);
    (void)host_.emitEvent("battle_turn_ended", payload);
}

void ScriptEventBridge::onBattleUnitDied(const game::defs::BattleUnitDiedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "battle_unit_died");
    populateBattleUnitFields(payload, host_.luaState(), event.unit, event.round_index);
    payload["source_unit_id"] = static_cast<int>(event.source_unit_id);
    payload["source_action_type"] = std::string{battleActionTypeName(event.source_action_type)};
    setOptionalString(payload, "skill_id", event.skill_id);
    setOptionalString(payload, "item_id", event.item_id);
    (void)host_.emitEvent("battle_unit_died", payload);
}

void ScriptEventBridge::onBattleSkillUsed(const game::defs::BattleSkillUsedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "battle_skill_used");
    populateBattleUnitFields(payload, host_.luaState(), event.unit, event.round_index);
    payload["skill_id"] = event.result.skill_id;
    setOptionalUnitId(payload, "target_unit_id", event.result.target_id);
    payload["result"] = battleActionResultToLua(host_.luaState(), event.result);
    (void)host_.emitEvent("battle_skill_used", payload);
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

void ScriptEventBridge::onMapEntered(const game::defs::MapEnteredEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "map_enter");
    setMapIdentity(registry_, payload, "map_id", "map_id_hash", event.map_id, event.map_name);
    setMapIdentity(registry_,
                   payload,
                   "previous_map_id",
                   "previous_map_id_hash",
                   event.previous_map_id,
                   event.previous_map_name);
    (void)host_.emitEvent("map_enter", payload);
}

void ScriptEventBridge::onMapExited(const game::defs::MapExitedEvent& event) {
    sol::table payload = host_.luaState().create_table();
    setEventName(payload, "map_exit");
    setMapIdentity(registry_, payload, "map_id", "map_id_hash", event.map_id, event.map_name);
    setMapIdentity(registry_,
                   payload,
                   "next_map_id",
                   "next_map_id_hash",
                   event.next_map_id,
                   event.next_map_name);
    (void)host_.emitEvent("map_exit", payload);
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
