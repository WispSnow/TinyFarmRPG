#include "script_game_api.h"

#include "engine/component/transform_component.h"
#include "engine/script/script_host.h"
#include "game/data/game_time.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/commands_inventory.h"
#include "game/defs/events_dialogue.h"
#include "game/system/system_helpers.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

namespace game::script {

namespace {

[[nodiscard]] std::optional<game::defs::DialogueChannel> parseDialogueChannel(const int value) {
    switch (value) {
    case 0:
        return game::defs::DialogueChannel::Conversation;
    case 1:
        return game::defs::DialogueChannel::Notice;
    case 2:
        return game::defs::DialogueChannel::ItemNotice;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] entt::id_type hashId(const std::string_view value) {
    return value.empty() ? entt::id_type{entt::null} : entt::hashed_string{value.data(), value.size()}.value();
}

} // namespace

ScriptGameApi::ScriptGameApi(engine::script::ScriptHost& host,
                             entt::registry& registry,
                             entt::dispatcher& dispatcher)
    : host_(host),
      registry_(registry),
      dispatcher_(dispatcher) {
}

std::uint32_t ScriptGameApi::day() const {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    return game_time ? game_time->day_ : 0U;
}

float ScriptGameApi::hour() const {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    return game_time ? game_time->hour_ : 0.0F;
}

float ScriptGameApi::minute() const {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    return game_time ? game_time->minute_ : 0.0F;
}

std::string ScriptGameApi::formattedTime() const {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        return "Day 0, 00:00";
    }
    return game_time->getFormattedTime();
}

bool ScriptGameApi::playerExists() const {
    return game::system::helpers::getPlayerEntity(registry_) != entt::null;
}

std::optional<engine::script::ScriptEntityHandle> ScriptGameApi::playerHandle() const {
    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return std::nullopt;
    }
    return host_.makeHandle(player);
}

std::tuple<float, float> ScriptGameApi::playerPosition() const {
    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return {0.0F, 0.0F};
    }

    const auto* transform = registry_.try_get<engine::component::TransformComponent>(player);
    if (!transform) {
        return {0.0F, 0.0F};
    }

    return {transform->position_.x, transform->position_.y};
}

bool ScriptGameApi::addItem(const std::string_view item_id,
                            const int count,
                            const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                            const int preferred_slot) {
    if (item_id.empty() || count <= 0) {
        spdlog::warn("ScriptHost: add_item 参数无效 item_id='{}', count={}", item_id, count);
        return false;
    }

    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.command.add_item", target, true)) {
        return false;
    }

    dispatcher_.trigger(game::defs::AddItemCommand{
        target,
        hashId(item_id),
        count,
        preferred_slot});
    return true;
}

bool ScriptGameApi::removeItem(const std::string_view item_id,
                               const int count,
                               const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                               const int slot_index) {
    if (item_id.empty() || count <= 0) {
        spdlog::warn("ScriptHost: remove_item 参数无效 item_id='{}', count={}", item_id, count);
        return false;
    }

    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.command.remove_item", target, true)) {
        return false;
    }

    dispatcher_.trigger(game::defs::RemoveItemCommand{
        target,
        hashId(item_id),
        count,
        slot_index});
    return true;
}

bool ScriptGameApi::inventorySync(const std::optional<engine::script::ScriptEntityHandle>& target_handle) {
    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.command.inventory_sync", target, true)) {
        return false;
    }

    dispatcher_.trigger(game::defs::InventorySyncCommand{target});
    return true;
}

bool ScriptGameApi::hotbarSync(const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                               const bool full_sync) {
    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.command.hotbar_sync", target, true)) {
        return false;
    }

    dispatcher_.trigger(game::defs::HotbarSyncCommand{target, full_sync});
    return true;
}

bool ScriptGameApi::interact(const engine::script::ScriptEntityHandle& target_handle,
                             const std::optional<engine::script::ScriptEntityHandle>& player_handle) {
    entt::entity target = entt::null;
    if (!host_.validateHandle(target_handle, target, "tf.command.interact.target")) {
        return false;
    }

    entt::entity player = entt::null;
    if (player_handle.has_value()) {
        if (!host_.validateHandle(player_handle.value(), player, "tf.command.interact.player")) {
            return false;
        }
    } else {
        player = game::system::helpers::getPlayerEntity(registry_);
        if (player == entt::null) {
            spdlog::warn("ScriptHost: tf.command.interact 失败，未找到默认玩家实体");
            return false;
        }
    }

    dispatcher_.trigger(game::defs::InteractCommand{player, target});
    return true;
}

bool ScriptGameApi::showDialogue(const std::string_view text,
                                 const std::string_view speaker,
                                 const int channel,
                                 const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                                 const std::string_view speaker_actor_id) {
    if (text.empty()) {
        return false;
    }

    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.dialogue.show", target, false)) {
        return false;
    }

    const auto resolved_channel = parseDialogueChannel(channel);
    if (!resolved_channel.has_value()) {
        return false;
    }

    game::defs::DialogueShowEvent evt{};
    evt.target = target;
    evt.speaker = std::string{speaker};
    evt.text = std::string{text};
    evt.world_position = target == entt::null
                             ? glm::vec2{0.0F}
                             : game::system::helpers::computeHeadPosition(registry_, target);
    evt.channel = *resolved_channel;
    if (!speaker_actor_id.empty()) {
        evt.speaker_actor_id = std::string{speaker_actor_id};
        evt.speaker_actor_id_hash = hashId(speaker_actor_id);
    }

    dispatcher_.trigger(evt);
    return true;
}

bool ScriptGameApi::hideDialogue(const int channel,
                                 const std::optional<engine::script::ScriptEntityHandle>& target_handle) {
    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.dialogue.hide", target, false)) {
        return false;
    }

    const auto resolved_channel = parseDialogueChannel(channel);
    if (!resolved_channel.has_value()) {
        return false;
    }

    game::defs::DialogueHideEvent evt{};
    evt.target = target;
    evt.channel = *resolved_channel;
    dispatcher_.enqueue(evt);
    return true;
}

bool ScriptGameApi::resolveTargetEntity(
    const std::optional<engine::script::ScriptEntityHandle>& raw_target,
    const std::string_view api_name,
    entt::entity& out_target,
    const bool require_default_player) const {
    if (raw_target.has_value()) {
        return host_.validateHandle(raw_target.value(), out_target, api_name);
    }

    out_target = game::system::helpers::getPlayerEntity(registry_);
    if (out_target == entt::null && require_default_player) {
        spdlog::warn("ScriptHost: {} 失败，未找到默认玩家实体", api_name);
        return false;
    }
    return true;
}

} // namespace game::script
