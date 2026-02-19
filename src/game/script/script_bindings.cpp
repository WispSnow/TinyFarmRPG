#include "script_bindings.h"

#include "engine/component/transform_component.h"
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
#include <tuple>

namespace {

/// 解析 Lua 侧可选的 target_id 参数：
/// - 有值时验证实体有效性后返回
/// - 无值时默认返回玩家实体
/// 大多数命令绑定都复用此函数，使 Lua 脚本可省略 target_id 来简化调用。
[[nodiscard]] entt::entity resolveTargetEntity(entt::registry& registry, sol::optional<std::uint32_t> raw_target) {
    if (raw_target.has_value()) {
        const entt::entity target = static_cast<entt::entity>(raw_target.value());
        if (registry.valid(target)) {
            return target;
        }
        spdlog::warn("ScriptHost: 指定目标实体无效: {}", raw_target.value());
        return entt::null;
    }
    return game::system::helpers::getPlayerEntity(registry);
}

/// 将 Lua 传入的 int 钳位到 [0, 255] 范围内，安全转为 uint8 对话频道号。
[[nodiscard]] std::uint8_t sanitizeChannel(int value) {
    const int clamped = std::clamp(value, 0, 255);
    return static_cast<std::uint8_t>(clamped);
}

} // namespace

namespace game::script {

void bindScriptAPI(sol::state& lua, entt::registry& registry, entt::dispatcher& dispatcher) {
    // 创建顶层命名空间 "tf"，所有游戏 API 挂载在此表下（tf.time / tf.player / ...）
    sol::table tf = lua.create_named_table("tf");

    // ── tf.time ── 只读查询游戏内时间 ──
    sol::table time_api = lua.create_table();
    // Lambda 通过引用捕获 registry；ctx().find 返回指针，
    // 找不到 GameTime 时返回安全默认值，不会崩溃。
    time_api.set_function("day", [&registry]() -> std::uint32_t {
        const auto* game_time = registry.ctx().find<game::data::GameTime>();
        return game_time ? game_time->day_ : 0u;
    });
    time_api.set_function("hour", [&registry]() -> float {
        const auto* game_time = registry.ctx().find<game::data::GameTime>();
        return game_time ? game_time->hour_ : 0.0f;
    });
    time_api.set_function("minute", [&registry]() -> float {
        const auto* game_time = registry.ctx().find<game::data::GameTime>();
        return game_time ? game_time->minute_ : 0.0f;
    });
    time_api.set_function("formatted", [&registry]() -> std::string {
        const auto* game_time = registry.ctx().find<game::data::GameTime>();
        if (!game_time) {
            return "Day 0, 00:00";
        }
        return game_time->getFormattedTime();
    });
    tf["time"] = time_api;

    // ── tf.player ── 只读查询玩家实体状态 ──
    sol::table player_api = lua.create_table();
    player_api.set_function("exists", [&registry]() -> bool {
        return game::system::helpers::getPlayerEntity(registry) != entt::null;
    });
    player_api.set_function("id", [&registry]() -> std::uint32_t {
        return static_cast<std::uint32_t>(game::system::helpers::getPlayerEntity(registry));
    });
    // 返回 std::tuple<float,float>，Sol2 自动映射为 Lua 多返回值：
    //   local x, y = tf.player.position()
    player_api.set_function("position", [&registry]() -> std::tuple<float, float> {
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
    tf["player"] = player_api;

    // ── tf.command ── 通过 dispatcher.trigger 同步发射命令 ──
    // 命令由对应的 C++ System 订阅处理，脚本不直接修改 ECS 数据。
    sol::table command_api = lua.create_table();
    // sol::optional<T> 映射 Lua 的可选参数（不传时为 nil → has_value() == false）。
    // 这样 Lua 侧可以 tf.command.add_item("wheat_seed", 5) 省略后两个参数。
    command_api.set_function(
        "add_item",
        [&registry, &dispatcher](const std::string& item_id,
                                 int count,
                                 sol::optional<std::uint32_t> target_id,
                                 sol::optional<int> preferred_slot) -> bool {
            if (item_id.empty() || count <= 0) {
                spdlog::warn("ScriptHost: add_item 参数无效 item_id='{}', count={}", item_id, count);
                return false;
            }

            const entt::entity target = resolveTargetEntity(registry, target_id);
            if (target == entt::null) {
                return false;
            }

            // Lua 字符串 → hashed_string → entt::id_type，与 ECS 组件中的物品 ID 格式一致
            dispatcher.trigger(game::defs::AddItemCommand{
                target,
                entt::hashed_string{item_id.c_str()}.value(),
                count,
                preferred_slot.value_or(-1)});
            return true;
        });
    command_api.set_function(
        "remove_item",
        [&registry, &dispatcher](const std::string& item_id,
                                 int count,
                                 sol::optional<std::uint32_t> target_id,
                                 sol::optional<int> slot_index) -> bool {
            if (item_id.empty() || count <= 0) {
                spdlog::warn("ScriptHost: remove_item 参数无效 item_id='{}', count={}", item_id, count);
                return false;
            }

            const entt::entity target = resolveTargetEntity(registry, target_id);
            if (target == entt::null) {
                return false;
            }

            dispatcher.trigger(game::defs::RemoveItemCommand{
                target,
                entt::hashed_string{item_id.c_str()}.value(),
                count,
                slot_index.value_or(-1)});
            return true;
        });
    command_api.set_function(
        "inventory_sync",
        [&registry, &dispatcher](sol::optional<std::uint32_t> target_id) -> bool {
            const entt::entity target = resolveTargetEntity(registry, target_id);
            if (target == entt::null) {
                return false;
            }
            dispatcher.trigger(game::defs::InventorySyncCommand{target});
            return true;
        });
    command_api.set_function(
        "hotbar_sync",
        [&registry, &dispatcher](sol::optional<std::uint32_t> target_id,
                                 sol::optional<bool> full_sync) -> bool {
            const entt::entity target = resolveTargetEntity(registry, target_id);
            if (target == entt::null) {
                return false;
            }
            dispatcher.trigger(game::defs::HotbarSyncCommand{target, full_sync.value_or(true)});
            return true;
        });
    // interact 的 target_id 是必填参数（不用 sol::optional），
    // 因为交互必须指定明确目标；player_id 可选，默认用当前玩家。
    command_api.set_function(
        "interact",
        [&registry, &dispatcher](std::uint32_t target_id,
                                 sol::optional<std::uint32_t> player_id) -> bool {
            const entt::entity target = static_cast<entt::entity>(target_id);
            if (!registry.valid(target)) {
                spdlog::warn("ScriptHost: interact 目标实体无效: {}", target_id);
                return false;
            }

            entt::entity player = game::system::helpers::getPlayerEntity(registry);
            if (player_id.has_value()) {
                const entt::entity explicit_player = static_cast<entt::entity>(player_id.value());
                if (registry.valid(explicit_player)) {
                    player = explicit_player;
                }
            }
            if (player == entt::null) {
                return false;
            }

            dispatcher.trigger(game::defs::InteractCommand{player, target});
            return true;
        });
    tf["command"] = command_api;

    // ── tf.dialogue ── 通过 dispatcher.enqueue 延迟发射 UI 事件 ──
    // 与 command 的 trigger（同步）不同，对话事件使用 enqueue（下一帧处理），
    // 避免在当前帧中途修改 UI 状态。
    sol::table dialogue_api = lua.create_table();
    dialogue_api.set_function(
        "show",
        [&registry, &dispatcher](const std::string& text,
                                 sol::optional<std::string> speaker,
                                 sol::optional<int> channel,
                                 sol::optional<std::uint32_t> target_id) -> bool {
            if (text.empty()) {
                return false;
            }

            const entt::entity target = resolveTargetEntity(registry, target_id);
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
            dispatcher.enqueue(evt);
            return true;
        });
    dialogue_api.set_function(
        "hide",
        [&registry, &dispatcher](sol::optional<int> channel,
                                 sol::optional<std::uint32_t> target_id) -> bool {
            game::defs::DialogueHideEvent evt{};
            evt.target = resolveTargetEntity(registry, target_id);
            evt.channel = sanitizeChannel(channel.value_or(1));
            dispatcher.enqueue(evt);
            return true;
        });
    tf["dialogue"] = dialogue_api;
}

} // namespace game::script
