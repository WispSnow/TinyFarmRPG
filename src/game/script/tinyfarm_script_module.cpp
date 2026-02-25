#include "tinyfarm_script_module.h"

#include "engine/component/transform_component.h"
#include "engine/script/script_binding_utils.h"
#include "engine/script/script_entity_handle.h"
#include "engine/script/script_host.h"
#include "game/data/game_time.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/system/system_helpers.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

namespace {

[[nodiscard]] bool resolveTargetEntity(engine::script::ScriptHost& host,
                                       entt::registry& registry,
                                       const sol::optional<engine::script::ScriptEntityHandle>& raw_target,
                                       std::string_view api_name,
                                       entt::entity& out_target,
                                       bool require_default_player) {
    if (raw_target.has_value()) {
        return host.validateHandle(raw_target.value(), out_target, api_name);
    }

    out_target = game::system::helpers::getPlayerEntity(registry);
    if (out_target == entt::null && require_default_player) {
        spdlog::warn("ScriptHost: {} 失败，未找到默认玩家实体", api_name);
        return false;
    }
    return true;
}

[[nodiscard]] std::uint8_t sanitizeChannel(int value) {
    const int clamped = std::clamp(value, 0, 255);
    return static_cast<std::uint8_t>(clamped);
}

} // namespace

namespace game::script {

void installTinyFarmScriptModule(sol::state& lua,
                                 engine::script::ScriptHost& host,
                                 entt::registry& registry,
                                 entt::dispatcher& dispatcher) {
    using engine::script::ScriptEntityHandle;

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
    time_impl.set_function("day", [&registry]() -> std::uint32_t {
        const auto* game_time = registry.ctx().find<game::data::GameTime>();
        return game_time ? game_time->day_ : 0u;
    });
    time_impl.set_function("hour", [&registry]() -> float {
        const auto* game_time = registry.ctx().find<game::data::GameTime>();
        return game_time ? game_time->hour_ : 0.0f;
    });
    time_impl.set_function("minute", [&registry]() -> float {
        const auto* game_time = registry.ctx().find<game::data::GameTime>();
        return game_time ? game_time->minute_ : 0.0f;
    });
    time_impl.set_function("formatted", [&registry]() -> std::string {
        const auto* game_time = registry.ctx().find<game::data::GameTime>();
        if (!game_time) {
            return "Day 0, 00:00";
        }
        return game_time->getFormattedTime();
    });
    tf_impl["time"] = engine::script::createReadOnlyProxy(lua, time_impl, "tf.time");

    // ── tf.player ──
    sol::table player_impl = lua.create_table();
    player_impl.set_function("exists", [&registry]() -> bool {
        return game::system::helpers::getPlayerEntity(registry) != entt::null;
    });
    player_impl.set_function("handle", [&registry, &host]() -> sol::optional<ScriptEntityHandle> {
        const entt::entity player = game::system::helpers::getPlayerEntity(registry);
        if (player == entt::null) {
            return {};
        }
        return host.makeHandle(player);
    });
    player_impl.set_function("position", [&registry]() -> std::tuple<float, float> {
        const entt::entity player = game::system::helpers::getPlayerEntity(registry);
        if (player == entt::null) {
            return {0.0f, 0.0f};
        }
        const auto* transform = registry.try_get<engine::component::TransformComponent>(player);
        if (!transform) {
            return {0.0f, 0.0f};
        }
        return {transform->position_.x, transform->position_.y};
    });
    tf_impl["player"] = engine::script::createReadOnlyProxy(lua, player_impl, "tf.player");

    // ── tf.command ──
    sol::table command_impl = lua.create_table();
    command_impl.set_function(
        "add_item",
        [&host, &registry, &dispatcher](const std::string& item_id,
                                        int count,
                                        sol::optional<ScriptEntityHandle> target_handle,
                                        sol::optional<int> preferred_slot) -> bool {
            if (item_id.empty() || count <= 0) {
                spdlog::warn("ScriptHost: add_item 参数无效 item_id='{}', count={}", item_id, count);
                return false;
            }

            entt::entity target = entt::null;
            if (!resolveTargetEntity(host, registry, target_handle, "tf.command.add_item", target, true)) {
                return false;
            }

            dispatcher.trigger(game::defs::AddItemCommand{
                target,
                entt::hashed_string{item_id.c_str()}.value(),
                count,
                preferred_slot.value_or(-1)});
            return true;
        });
    command_impl.set_function(
        "remove_item",
        [&host, &registry, &dispatcher](const std::string& item_id,
                                        int count,
                                        sol::optional<ScriptEntityHandle> target_handle,
                                        sol::optional<int> slot_index) -> bool {
            if (item_id.empty() || count <= 0) {
                spdlog::warn("ScriptHost: remove_item 参数无效 item_id='{}', count={}", item_id, count);
                return false;
            }

            entt::entity target = entt::null;
            if (!resolveTargetEntity(host, registry, target_handle, "tf.command.remove_item", target, true)) {
                return false;
            }

            dispatcher.trigger(game::defs::RemoveItemCommand{
                target,
                entt::hashed_string{item_id.c_str()}.value(),
                count,
                slot_index.value_or(-1)});
            return true;
        });
    command_impl.set_function(
        "inventory_sync",
        [&host, &registry, &dispatcher](sol::optional<ScriptEntityHandle> target_handle) -> bool {
            entt::entity target = entt::null;
            if (!resolveTargetEntity(host, registry, target_handle, "tf.command.inventory_sync", target, true)) {
                return false;
            }
            dispatcher.trigger(game::defs::InventorySyncCommand{target});
            return true;
        });
    command_impl.set_function(
        "hotbar_sync",
        [&host, &registry, &dispatcher](sol::optional<ScriptEntityHandle> target_handle,
                                        sol::optional<bool> full_sync) -> bool {
            entt::entity target = entt::null;
            if (!resolveTargetEntity(host, registry, target_handle, "tf.command.hotbar_sync", target, true)) {
                return false;
            }
            dispatcher.trigger(game::defs::HotbarSyncCommand{target, full_sync.value_or(true)});
            return true;
        });
    command_impl.set_function(
        "interact",
        [&host, &registry, &dispatcher](const ScriptEntityHandle& target_handle,
                                        sol::optional<ScriptEntityHandle> player_handle) -> bool {
            entt::entity target = entt::null;
            if (!host.validateHandle(target_handle, target, "tf.command.interact.target")) {
                return false;
            }

            entt::entity player = entt::null;
            if (player_handle.has_value()) {
                if (!host.validateHandle(player_handle.value(), player, "tf.command.interact.player")) {
                    return false;
                }
            } else {
                player = game::system::helpers::getPlayerEntity(registry);
                if (player == entt::null) {
                    spdlog::warn("ScriptHost: tf.command.interact 失败，未找到默认玩家实体");
                    return false;
                }
            }

            dispatcher.trigger(game::defs::InteractCommand{player, target});
            return true;
        });
    tf_impl["command"] = engine::script::createReadOnlyProxy(lua, command_impl, "tf.command");

    // ── tf.dialogue ──
    sol::table dialogue_impl = lua.create_table();
    dialogue_impl.set_function(
        "show",
        [&host, &registry, &dispatcher](const std::string& text,
                                        sol::optional<std::string> speaker,
                                        sol::optional<int> channel,
                                        sol::optional<ScriptEntityHandle> target_handle) -> bool {
            if (text.empty()) {
                return false;
            }

            entt::entity target = entt::null;
            if (!resolveTargetEntity(host, registry, target_handle, "tf.dialogue.show", target, false)) {
                return false;
            }

            const std::uint8_t resolved_channel = sanitizeChannel(channel.value_or(1));
            const glm::vec2 world_position = target == entt::null
                                                 ? glm::vec2{0.0f}
                                                 : game::system::helpers::computeHeadPosition(registry, target);

            game::defs::DialogueShowEvent evt{};
            evt.target = target;
            evt.speaker = speaker.value_or("Script");
            evt.text = text;
            evt.world_position = world_position;
            evt.channel = resolved_channel;
            dispatcher.trigger(evt);
            return true;
        });
    dialogue_impl.set_function(
        "hide",
        [&host, &registry, &dispatcher](sol::optional<int> channel,
                                        sol::optional<ScriptEntityHandle> target_handle) -> bool {
            entt::entity target = entt::null;
            if (!resolveTargetEntity(host, registry, target_handle, "tf.dialogue.hide", target, false)) {
                return false;
            }

            game::defs::DialogueHideEvent evt{};
            evt.target = target;
            evt.channel = sanitizeChannel(channel.value_or(1));
            dispatcher.enqueue(evt);
            return true;
        });
    tf_impl["dialogue"] = engine::script::createReadOnlyProxy(lua, dialogue_impl, "tf.dialogue");

    lua["tf"] = engine::script::createReadOnlyProxy(lua, tf_impl, "tf");
}

} // namespace game::script
