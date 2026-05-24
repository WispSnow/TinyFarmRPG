#include "tinyfarm_script_module.h"

#include "engine/script/script_binding_utils.h"
#include "engine/script/script_entity_handle.h"
#include "engine/script/script_host.h"
#include "game/script/script_game_api.h"
#include "game/script/script_state.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] std::optional<engine::script::ScriptEntityHandle> toStdOptional(
    const sol::optional<engine::script::ScriptEntityHandle>& value) {
    if (value.has_value()) {
        return value.value();
    }
    return std::nullopt;
}

[[nodiscard]] bool registerEventCallback(engine::script::ScriptHost& host,
                                         const std::string_view event_name,
                                         const sol::object& callback) {
    if (event_name.empty() || !callback.is<sol::function>()) {
        return false;
    }

    sol::protected_function protected_callback = callback.as<sol::function>();
    return host.registerEventCallback(event_name, std::move(protected_callback));
}

[[nodiscard]] game::script::ScriptStateStore& scriptState(entt::registry& registry) {
    if (auto* state = registry.ctx().find<game::script::ScriptStateStore>()) {
        return *state;
    }
    return registry.ctx().emplace<game::script::ScriptStateStore>();
}

[[nodiscard]] bool isValidScriptStateKey(const std::string& key) {
    return !key.empty();
}

[[nodiscard]] sol::object scriptStateValueToLua(sol::state& lua, const game::script::ScriptStateValue& value) {
    return std::visit(
        [&lua](const auto& stored_value) -> sol::object {
            using Value = std::decay_t<decltype(stored_value)>;
            if constexpr (std::is_same_v<Value, std::nullptr_t>) {
                return sol::make_object(lua, sol::lua_nil);
            } else {
                return sol::make_object(lua, stored_value);
            }
        },
        value);
}

[[nodiscard]] std::optional<game::script::ScriptStateValue> luaToScriptStateValue(const sol::object& value) {
    switch (value.get_type()) {
        case sol::type::lua_nil:
            return game::script::ScriptStateValue{nullptr};
        case sol::type::boolean:
            return game::script::ScriptStateValue{value.as<bool>()};
        case sol::type::number: {
            const double number = value.as<double>();
            if (!std::isfinite(number)) {
                return std::nullopt;
            }
            return game::script::ScriptStateValue{number};
        }
        case sol::type::string:
            return game::script::ScriptStateValue{value.as<std::string>()};
        default:
            return std::nullopt;
    }
}

void warnScriptStateTypeMismatch(const std::string& key, std::string_view expected_type) {
    spdlog::warn("tf.state: key '{}' is not {}", key, expected_type);
}

[[nodiscard]] sol::table commandResultToLua(sol::state& lua, const game::script::ScriptCommandResult& result) {
    sol::table table = lua.create_table();
    table["ok"] = result.ok;
    table["reason"] = result.reason;
    return table;
}

} // namespace

namespace game::script {

void installTinyFarmScriptModule(sol::state& lua,
                                 engine::script::ScriptHost& host,
                                 entt::registry& registry,
                                 entt::dispatcher& dispatcher) {
    using engine::script::ScriptEntityHandle;
    const auto api = std::make_shared<game::script::ScriptGameApi>(host, registry, dispatcher);

    lua.new_usertype<engine::script::ScriptEntityHandle>(
        "ScriptEntityHandle",
        sol::no_constructor,
        "entity_id",
        sol::readonly_property(
            [](const ScriptEntityHandle& handle) { return engine::script::toRawEntity(handle); }),
        "is_valid",
        [](const ScriptEntityHandle& handle) { return !engine::script::isNullHandle(handle); });

    sol::table tf_impl = lua.create_table();

    // ── tf.time ──
    sol::table time_impl = lua.create_table();
    time_impl.set_function("day", [api]() -> std::uint32_t { return api->day(); });
    time_impl.set_function("hour", [api]() -> float { return api->hour(); });
    time_impl.set_function("minute", [api]() -> float { return api->minute(); });
    time_impl.set_function("formatted", [api]() -> std::string { return api->formattedTime(); });
    tf_impl["time"] = engine::script::createReadOnlyProxy(lua, time_impl, "tf.time");

    // ── tf.player ──
    sol::table player_impl = lua.create_table();
    player_impl.set_function("exists", [api]() -> bool { return api->playerExists(); });
    player_impl.set_function("handle", [api]() -> sol::optional<ScriptEntityHandle> {
        const auto handle = api->playerHandle();
        if (!handle.has_value()) {
            return {};
        }
        return handle.value();
    });
    player_impl.set_function("position", [api]() -> std::tuple<float, float> { return api->playerPosition(); });
    tf_impl["player"] = engine::script::createReadOnlyProxy(lua, player_impl, "tf.player");

    // ── tf.entity ──
    sol::table entity_impl = lua.create_table();
    entity_impl.set_function("actor_id", [api](const ScriptEntityHandle& handle) -> sol::optional<std::string> {
        const auto actor_id = api->entityActorId(handle);
        if (!actor_id.has_value()) {
            return {};
        }
        return actor_id.value();
    });
    entity_impl.set_function("name", [api](const ScriptEntityHandle& handle) -> sol::optional<std::string> {
        const auto name = api->entityName(handle);
        if (!name.has_value()) {
            return {};
        }
        return name.value();
    });
    entity_impl.set_function("position", [api](const ScriptEntityHandle& handle) -> std::tuple<float, float> {
        return api->entityPosition(handle);
    });
    entity_impl.set_function("has_component", [api](const ScriptEntityHandle& handle, const std::string& kind) -> bool {
        return api->entityHasComponent(handle, kind);
    });
    tf_impl["entity"] = engine::script::createReadOnlyProxy(lua, entity_impl, "tf.entity");

    // ── tf.quest ──
    sol::table quest_impl = lua.create_table();
    quest_impl.set_function("status", [api](const std::string& quest_id) -> std::string {
        return api->questStatus(quest_id);
    });
    quest_impl.set_function(
        "progress",
        [&lua, api](const std::string& quest_id, const std::string& objective_id) -> sol::table {
            const auto progress = api->questProgress(quest_id, objective_id);
            sol::table table = lua.create_table();
            table["current"] = progress.current;
            table["required"] = progress.required;
            return table;
        });
    quest_impl.set_function("is_available", [api](const std::string& quest_id) -> bool {
        return api->questIsAvailable(quest_id);
    });
    quest_impl.set_function(
        "accept",
        [&lua, api](const std::string& quest_id, sol::optional<ScriptEntityHandle> giver_handle) -> sol::table {
            return commandResultToLua(lua, api->questAccept(quest_id, toStdOptional(giver_handle)));
        });
    quest_impl.set_function(
        "turn_in",
        [&lua, api](const std::string& quest_id, sol::optional<ScriptEntityHandle> giver_handle) -> sol::table {
            return commandResultToLua(lua, api->questTurnIn(quest_id, toStdOptional(giver_handle)));
        });
    tf_impl["quest"] = engine::script::createReadOnlyProxy(lua, quest_impl, "tf.quest");

    // ── tf.party ──
    sol::table party_impl = lua.create_table();
    party_impl.set_function("members", [&lua, api]() -> sol::table {
        const std::vector<std::string> members = api->partyMembers();
        sol::table table = lua.create_table();
        int index = 1;
        for (const auto& actor_id : members) {
            table[index++] = actor_id;
        }
        return table;
    });
    party_impl.set_function("is_recruited", [api](const std::string& actor_id) -> bool {
        return api->partyIsRecruited(actor_id);
    });
    party_impl.set_function(
        "request_recruit",
        [&lua, api](const std::string& actor_id,
                    sol::optional<ScriptEntityHandle> recruiter_handle) -> sol::table {
            return commandResultToLua(lua, api->partyRequestRecruit(actor_id, toStdOptional(recruiter_handle)));
        });
    party_impl.set_function("level", [api](const std::string& actor_id) -> int {
        return api->partyLevel(actor_id);
    });
    party_impl.set_function("initial_level", [api](const std::string& actor_id) -> int {
        return api->partyInitialLevel(actor_id);
    });
    tf_impl["party"] = engine::script::createReadOnlyProxy(lua, party_impl, "tf.party");

    // ── tf.shop ──
    sol::table shop_impl = lua.create_table();
    shop_impl.set_function(
        "open",
        [&lua, api](const std::string& shop_id, sol::optional<ScriptEntityHandle> merchant_handle) -> sol::table {
            return commandResultToLua(lua, api->shopOpen(shop_id, toStdOptional(merchant_handle)));
        });
    tf_impl["shop"] = engine::script::createReadOnlyProxy(lua, shop_impl, "tf.shop");

    // ── tf.battle ──
    sol::table battle_impl = lua.create_table();
    battle_impl.set_function(
        "start",
        [&lua, api](sol::optional<std::string> troop_id, sol::optional<sol::table> options) -> sol::table {
            std::vector<std::string> actor_ids{};
            std::string battle_background_id{};
            if (options.has_value()) {
                const sol::object actor_ids_obj = options.value().get<sol::object>("actor_ids");
                if (actor_ids_obj.is<sol::table>()) {
                    sol::table actor_ids_table = actor_ids_obj.as<sol::table>();
                    for (int index = 1;; ++index) {
                        const sol::object actor_id_obj = actor_ids_table.get<sol::object>(index);
                        if (!actor_id_obj.valid() || actor_id_obj.get_type() == sol::type::lua_nil) {
                            break;
                        }
                        if (actor_id_obj.is<std::string>()) {
                            actor_ids.push_back(actor_id_obj.as<std::string>());
                        }
                    }
                }

                const sol::object background_obj = options.value().get<sol::object>("battle_background_id");
                if (background_obj.is<std::string>()) {
                    battle_background_id = background_obj.as<std::string>();
                }
            }

            return commandResultToLua(
                lua,
                api->battleStart(troop_id.value_or(""), std::move(actor_ids), battle_background_id));
        });
    tf_impl["battle"] = engine::script::createReadOnlyProxy(lua, battle_impl, "tf.battle");

    // ── tf.map ──
    sol::table map_impl = lua.create_table();
    map_impl.set_function("current", [api]() -> std::string {
        return api->currentMap();
    });
    tf_impl["map"] = engine::script::createReadOnlyProxy(lua, map_impl, "tf.map");

    // ── tf.command ──
    sol::table command_impl = lua.create_table();
    command_impl.set_function(
        "add_item",
        [api](const std::string& item_id,
              int count,
              sol::optional<ScriptEntityHandle> target_handle,
              sol::optional<int> preferred_slot) -> bool {
            return api->addItem(item_id, count, toStdOptional(target_handle), preferred_slot.value_or(-1));
        });
    command_impl.set_function(
        "remove_item",
        [api](const std::string& item_id,
              int count,
              sol::optional<ScriptEntityHandle> target_handle,
              sol::optional<int> slot_index) -> bool {
            return api->removeItem(item_id, count, toStdOptional(target_handle), slot_index.value_or(-1));
        });
    command_impl.set_function(
        "inventory_sync",
        [api](sol::optional<ScriptEntityHandle> target_handle) -> bool {
            return api->inventorySync(toStdOptional(target_handle));
        });
    command_impl.set_function(
        "hotbar_sync",
        [api](sol::optional<ScriptEntityHandle> target_handle,
              sol::optional<bool> full_sync) -> bool {
            return api->hotbarSync(toStdOptional(target_handle), full_sync.value_or(true));
        });
    command_impl.set_function(
        "interact",
        [api](const ScriptEntityHandle& target_handle,
              sol::optional<ScriptEntityHandle> player_handle) -> bool {
            return api->interact(target_handle, toStdOptional(player_handle));
        });
    tf_impl["command"] = engine::script::createReadOnlyProxy(lua, command_impl, "tf.command");

    // ── tf.dialogue ──
    sol::table dialogue_impl = lua.create_table();
    dialogue_impl.set_function(
        "show",
        [api](const std::string& text,
              sol::optional<std::string> speaker,
              sol::optional<int> channel,
              sol::optional<ScriptEntityHandle> target_handle,
              sol::optional<std::string> speaker_actor_id) -> bool {
            return api->showDialogue(
                text,
                speaker.value_or("Script"),
                channel.value_or(1),
                toStdOptional(target_handle),
                speaker_actor_id.value_or(""));
        });
    dialogue_impl.set_function(
        "hide",
        [api](sol::optional<int> channel,
              sol::optional<ScriptEntityHandle> target_handle) -> bool {
            return api->hideDialogue(channel.value_or(1), toStdOptional(target_handle));
        });
    tf_impl["dialogue"] = engine::script::createReadOnlyProxy(lua, dialogue_impl, "tf.dialogue");

    // ── tf.event / tf.callbacks ──
    sol::table event_impl = lua.create_table();
    event_impl.set_function("on", [&host](const std::string& event_name, const sol::object& callback) -> bool {
        return registerEventCallback(host, event_name, callback);
    });
    tf_impl["event"] = engine::script::createReadOnlyProxy(lua, event_impl, "tf.event");

    sol::table callbacks_impl = lua.create_table();
    callbacks_impl.set_function("on_interact", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "interact", callback);
    });
    callbacks_impl.set_function("on_dialogue_closed", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "dialogue_closed", callback);
    });
    callbacks_impl.set_function("on_battle_start", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "battle_started", callback);
    });
    callbacks_impl.set_function("on_battle_end", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "battle_ended", callback);
    });
    callbacks_impl.set_function("on_day_changed", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "day_changed", callback);
    });
    callbacks_impl.set_function("on_time_of_day_changed", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "time_of_day_changed", callback);
    });
    callbacks_impl.set_function("on_inventory_changed", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "inventory_changed", callback);
    });
    callbacks_impl.set_function("on_item_used", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "item_used", callback);
    });
    callbacks_impl.set_function("on_quest_accepted", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "quest_accepted", callback);
    });
    callbacks_impl.set_function("on_quest_completed", [&host](const sol::object& callback) -> bool {
        return registerEventCallback(host, "quest_completed", callback);
    });
    tf_impl["callbacks"] = engine::script::createReadOnlyProxy(lua, callbacks_impl, "tf.callbacks");

    // ── tf.script ──
    sol::table script_impl = lua.create_table();
    script_impl.set_function("require", [&host](const std::string& module_name) -> sol::object {
        return host.requireScriptModule(module_name);
    });
    tf_impl["script"] = engine::script::createReadOnlyProxy(lua, script_impl, "tf.script");

    // ── tf.state ──
    sol::table state_impl = lua.create_table();
    state_impl.set_function("has", [&registry](const std::string& key) -> bool {
        return isValidScriptStateKey(key) && scriptState(registry).has(key);
    });
    state_impl.set_function("get",
                            [&lua, &registry](const std::string& key,
                                              sol::optional<sol::object> default_value) -> sol::object {
        if (!isValidScriptStateKey(key)) {
            return default_value.value_or(sol::make_object(lua, sol::lua_nil));
        }
        if (const auto* value = scriptState(registry).find(key)) {
            return scriptStateValueToLua(lua, *value);
        }
        return default_value.value_or(sol::make_object(lua, sol::lua_nil));
    });
    state_impl.set_function("get_number",
                            [&registry](const std::string& key, sol::optional<double> default_value) -> double {
        const double fallback = default_value.value_or(0.0);
        if (!isValidScriptStateKey(key)) {
            return fallback;
        }
        const auto* value = scriptState(registry).find(key);
        if (!value) {
            return fallback;
        }
        if (const auto* number = std::get_if<double>(value)) {
            return *number;
        }
        warnScriptStateTypeMismatch(key, "number");
        return fallback;
    });
    state_impl.set_function("get_int", [&registry](const std::string& key, sol::optional<int> default_value) -> int {
        const int fallback = default_value.value_or(0);
        if (!isValidScriptStateKey(key)) {
            return fallback;
        }
        const auto* value = scriptState(registry).find(key);
        if (!value) {
            return fallback;
        }
        const auto* number = std::get_if<double>(value);
        if (!number || !std::isfinite(*number) ||
            *number < static_cast<double>(std::numeric_limits<int>::min()) ||
            *number > static_cast<double>(std::numeric_limits<int>::max())) {
            warnScriptStateTypeMismatch(key, "int");
            return fallback;
        }
        return static_cast<int>(*number);
    });
    state_impl.set_function("get_bool", [&registry](const std::string& key, sol::optional<bool> default_value) -> bool {
        const bool fallback = default_value.value_or(false);
        if (!isValidScriptStateKey(key)) {
            return fallback;
        }
        const auto* value = scriptState(registry).find(key);
        if (!value) {
            return fallback;
        }
        if (const auto* boolean = std::get_if<bool>(value)) {
            return *boolean;
        }
        warnScriptStateTypeMismatch(key, "bool");
        return fallback;
    });
    state_impl.set_function(
        "get_string",
        [&registry](const std::string& key, sol::optional<std::string> default_value) -> std::string {
            const std::string fallback = default_value.value_or("");
            if (!isValidScriptStateKey(key)) {
                return fallback;
            }
            const auto* value = scriptState(registry).find(key);
            if (!value) {
                return fallback;
            }
            if (const auto* text = std::get_if<std::string>(value)) {
                return *text;
            }
            warnScriptStateTypeMismatch(key, "string");
            return fallback;
        });
    state_impl.set_function("set",
                            [&lua, &registry](const std::string& key, sol::optional<sol::object> value) -> bool {
        if (!isValidScriptStateKey(key)) {
            spdlog::warn("tf.state.set: key must not be empty");
            return false;
        }
        const auto parsed = luaToScriptStateValue(value.value_or(sol::make_object(lua, sol::lua_nil)));
        if (!parsed.has_value()) {
            spdlog::warn("tf.state.set: key '{}' rejected non-serializable value", key);
            return false;
        }
        scriptState(registry).set(key, *parsed);
        return true;
    });
    state_impl.set_function("add",
                            [&lua, &registry](const std::string& key,
                                              sol::optional<double> amount) -> sol::object {
        if (!isValidScriptStateKey(key)) {
            spdlog::warn("tf.state.add: key must not be empty");
            return sol::make_object(lua, sol::lua_nil);
        }
        const auto* current = scriptState(registry).find(key);
        if (current && !std::holds_alternative<double>(*current)) {
            warnScriptStateTypeMismatch(key, "number");
            return sol::make_object(lua, sol::lua_nil);
        }
        const double next_value = scriptState(registry).add(key, amount.value_or(1.0));
        return sol::make_object(lua, next_value);
    });
    state_impl.set_function("unset", [&registry](const std::string& key) -> bool {
        return isValidScriptStateKey(key) && scriptState(registry).erase(key);
    });
    tf_impl["state"] = engine::script::createReadOnlyProxy(lua, state_impl, "tf.state");

    lua["tf"] = engine::script::createReadOnlyProxy(lua, tf_impl, "tf");
}

} // namespace game::script
