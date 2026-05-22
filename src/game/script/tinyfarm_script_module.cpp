#include "tinyfarm_script_module.h"

#include "engine/script/script_binding_utils.h"
#include "engine/script/script_entity_handle.h"
#include "engine/script/script_host.h"
#include "game/script/script_game_api.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

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

    lua["tf"] = engine::script::createReadOnlyProxy(lua, tf_impl, "tf");
}

} // namespace game::script
