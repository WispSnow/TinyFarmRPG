#include "dialogue_system.h"
#include "system_helpers.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/name_component.h"
#include "game/defs/events.h"
#include <nlohmann/json.hpp>
#include <entt/entity/registry.hpp>
#include <entt/core/hashed_string.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <glm/geometric.hpp>
#include <limits>

namespace {
constexpr std::uint8_t DIALOGUE_CHANNEL = 0;
constexpr float DIALOGUE_CLOSE_DISTANCE_FACTOR = 1.4f;  // 对话中允许比触发距离再远 40% 才关闭
} // namespace

namespace game::system {

DialogueSystem::DialogueSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    dispatcher_.sink<game::defs::InteractRequest>().connect<&DialogueSystem::onInteractRequest>(this);
}

DialogueSystem::~DialogueSystem() {
    dispatcher_.disconnect(this);
}

bool DialogueSystem::loadDialogueFile(std::string_view file_path) {
    std::ifstream file(std::string{file_path});
    if (!file.is_open()) {
        spdlog::warn("DialogueSystem: 无法打开对话文件 {}", file_path);
        return false;
    }
    try {
        nlohmann::json json;
        file >> json;
        for (auto& [key, value] : json.items()) {
            if (!value.is_array()) continue;
            std::vector<std::string> lines;
            for (const auto& line : value) {
                lines.push_back(line.get<std::string>());
            }
            dialogue_table_.emplace(entt::hashed_string(key.c_str()), std::move(lines));
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("DialogueSystem: 解析对话文件失败 {} - {}", file_path, e.what());
        return false;
    }
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

    // 更新气泡位置
    if (active_entity_ != entt::null) {
        const auto head_pos = helpers::computeHeadPosition(registry_, active_entity_);
        helpers::emitDialogueBubbleMove(dispatcher_, DIALOGUE_CHANNEL, active_entity_, head_pos);
    }
}

void DialogueSystem::onInteractRequest(const game::defs::InteractRequest& event) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || event.player != player) return;
    if (event.target == entt::null || !registry_.valid(event.target)) return;

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

    const auto head_pos = helpers::computeHeadPosition(registry_, event.target);

    if (active_entity_ != event.target) {
        startDialogue(event.target, *dialogue, lines, head_pos);
        return;
    }

    if (advanceDialogue(event.target, *dialogue, lines, head_pos)) {
        return;
    }

    endDialogue(event.target, *dialogue);
}

void DialogueSystem::startDialogue(entt::entity entity,
                                  game::component::DialogueComponent& dialogue,
                                  const std::vector<std::string>& lines,
                                  glm::vec2 head_pos) {
    if (active_entity_ != entt::null && active_entity_ != entity) {
        closeDialogue(active_entity_);
    }
    dialogue.active_ = true;
    dialogue.current_line_ = 0;
    dialogue.cooldown_timer_ = dialogue.cooldown_;
    active_entity_ = entity;
    showLine(entity, lines, dialogue.current_line_, head_pos);
}

bool DialogueSystem::advanceDialogue(entt::entity entity,
                                    game::component::DialogueComponent& dialogue,
                                    const std::vector<std::string>& lines,
                                    glm::vec2 head_pos) {
    if (dialogue.current_line_ + 1 >= lines.size()) {
        return false;
    }
    dialogue.current_line_++;
    dialogue.cooldown_timer_ = dialogue.cooldown_;
    showLine(entity, lines, dialogue.current_line_, head_pos);
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
    helpers::emitDialogueBubbleHide(dispatcher_, DIALOGUE_CHANNEL, entity);
}

void DialogueSystem::showLine(entt::entity entity, const std::vector<std::string>& lines, std::size_t line_index, glm::vec2 head_pos) {
    if (line_index >= lines.size()) return;
    std::string speaker;
    if (auto* name = registry_.try_get<engine::component::NameComponent>(entity)) {
        speaker = name->name_;
    }
    helpers::emitDialogueBubbleShow(dispatcher_, DIALOGUE_CHANNEL, entity, std::move(speaker), lines[line_index], head_pos);
    registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
    if (auto* state = registry_.try_get<game::component::StateComponent>(entity)) {
        state->action_ = game::component::Action::Idle;
    }
}

} // namespace game::system
