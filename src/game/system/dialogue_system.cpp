#include "dialogue_system.h"
#include "system_helpers.h"
#include "game/component/npc_component.h"
#include "game/component/merchant_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "engine/utils/json_file_loader.h"
#include "engine/component/transform_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/name_component.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include <nlohmann/json.hpp>
#include <entt/entity/registry.hpp>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <glm/geometric.hpp>
#include <limits>

namespace {
constexpr game::defs::DialogueChannel DIALOGUE_CHANNEL = game::defs::DialogueChannel::Conversation;
constexpr float DIALOGUE_CLOSE_DISTANCE_FACTOR = 1.4f;  // 对话中允许比触发距离再远 40% 才关闭
} // namespace

namespace game::system {

DialogueSystem::DialogueSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    dispatcher_.sink<game::defs::InteractCommand>().connect<&DialogueSystem::onInteractCommand>(this);
}

DialogueSystem::~DialogueSystem() {
    dispatcher_.disconnect(this);
}

bool DialogueSystem::loadDialogueFile(std::string_view file_path) {
    nlohmann::json json;
    if (!engine::utils::loadJsonObjectFile(file_path, json, "DialogueSystem")) {
        return false;
    }

    std::unordered_map<entt::id_type, std::vector<std::string>> next_dialogue_table{};
    for (auto& [key, value] : json.items()) {
        if (!value.is_array()) {
            continue;
        }

        std::vector<std::string> lines;
        lines.reserve(value.size());
        for (const auto& line : value) {
            if (const auto* text = line.get_ptr<const nlohmann::json::string_t*>()) {
                lines.push_back(*text);
                continue;
            }
            spdlog::error("DialogueSystem: 对话 '{}' 包含非字符串行", key);
            return false;
        }
        next_dialogue_table.emplace(entt::hashed_string(key.c_str()), std::move(lines));
    }

    dialogue_table_ = std::move(next_dialogue_table);
    return true;
}

void DialogueSystem::update(float delta_time) {
    // 冷却计时
    auto cooldown_view = registry_.view<game::component::DialogueComponent>();
    for (auto entity : cooldown_view) {
        auto& dialogue = cooldown_view.get<game::component::DialogueComponent>(entity);
        dialogue.cooldown_timer_ = std::max(0.0f, dialogue.cooldown_timer_ - delta_time);
    }

    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        if (active_entity_ != entt::null) {
            closeDialogue(active_entity_);
        }
        return;
    }
    const auto& player_transform = registry_.get<engine::component::TransformComponent>(player);

    // 如果对话中的实体离得太远，关闭
    if (active_entity_ != entt::null) {
        auto* dialogue = registry_.try_get<game::component::DialogueComponent>(active_entity_);
        auto* transform = registry_.try_get<engine::component::TransformComponent>(active_entity_);
        if (!dialogue || !transform ||
            glm::distance(transform->position_, player_transform.position_) > dialogue->interact_distance_ * DIALOGUE_CLOSE_DISTANCE_FACTOR) {
            closeDialogue(active_entity_);
            active_entity_ = entt::null;
        }
    }

}

void DialogueSystem::onInteractCommand(const game::defs::InteractCommand& event) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || event.player != player) return;
    if (event.target == entt::null || !registry_.valid(event.target)) return;
    if (helpers::isScriptedInteraction(registry_, event.target)) return;
    if (registry_.all_of<game::component::MerchantComponent>(event.target)) return;
    if (registry_.all_of<game::component::QuestGiverComponent>(event.target)) return;
    if (registry_.all_of<game::component::RecruitableComponent>(event.target)) return;

    auto* dialogue = registry_.try_get<game::component::DialogueComponent>(event.target);
    if (!dialogue || dialogue->dialogue_id_ == entt::null) return;
    if (dialogue->cooldown_timer_ > 0.0f) return;
    if (auto* sleep = registry_.try_get<game::component::SleepRoutine>(event.target); sleep && sleep->is_sleeping_) return;

    auto lines_it = dialogue_table_.find(dialogue->dialogue_id_);
    if (lines_it == dialogue_table_.end() || lines_it->second.empty()) {
        spdlog::warn("DialogueSystem: 未找到对话文本 id={}", dialogue->dialogue_id_);
        return;
    }
    auto& lines = lines_it->second;

    if (active_entity_ != event.target) {
        startDialogue(event.target, *dialogue, lines);
        return;
    }

    if (advanceDialogue(event.target, *dialogue, lines)) {
        return;
    }

    endDialogue(event.target, *dialogue);
}

void DialogueSystem::startDialogue(entt::entity entity,
                                  game::component::DialogueComponent& dialogue,
                                  const std::vector<std::string>& lines) {
    if (active_entity_ != entt::null && active_entity_ != entity) {
        closeDialogue(active_entity_);
    }
    dialogue.active_ = true;
    dialogue.current_line_ = 0;
    dialogue.cooldown_timer_ = dialogue.cooldown_;
    active_entity_ = entity;
    showLine(entity, lines, dialogue.current_line_);
}

bool DialogueSystem::advanceDialogue(entt::entity entity,
                                    game::component::DialogueComponent& dialogue,
                                    const std::vector<std::string>& lines) {
    if (dialogue.current_line_ + 1 >= lines.size()) {
        return false;
    }
    dialogue.current_line_++;
    dialogue.cooldown_timer_ = dialogue.cooldown_;
    showLine(entity, lines, dialogue.current_line_);
    return true;
}

void DialogueSystem::endDialogue(entt::entity entity, game::component::DialogueComponent&) {
    closeDialogue(entity);
    if (active_entity_ == entity) {
        active_entity_ = entt::null;
    }
}

void DialogueSystem::closeDialogue(entt::entity entity) {
    if (auto* dialogue = registry_.try_get<game::component::DialogueComponent>(entity)) {
        dialogue->active_ = false;
        dialogue->current_line_ = 0;
    }
    helpers::emitDialogueHide(dispatcher_, DIALOGUE_CHANNEL, entity);
}

void DialogueSystem::showLine(entt::entity entity, const std::vector<std::string>& lines, std::size_t line_index) {
    if (line_index >= lines.size()) return;
    std::string speaker;
    if (auto* name = registry_.try_get<engine::component::NameComponent>(entity)) {
        speaker = name->name_;
    }
    helpers::emitDialogueShow(dispatcher_, DIALOGUE_CHANNEL, entity, std::move(speaker), lines[line_index]);
    registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
    if (auto* state = registry_.try_get<game::component::StateComponent>(entity)) {
        state->action_ = game::component::Action::Idle;
    }
}

} // namespace game::system
