#include "scripted_dialogue_lifecycle_system.h"

#include "engine/component/transform_component.h"
#include "game/component/npc_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/defs/events_dialogue.h"
#include "game/system/system_helpers.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <vector>

namespace {

constexpr float SCRIPTED_DIALOGUE_CLOSE_DISTANCE_FACTOR = 0.75F;
constexpr float SCRIPTED_DIALOGUE_CLOSE_DISTANCE_MAX_PX = 48.0F;

} // namespace

namespace game::system {

ScriptedDialogueLifecycleSystem::ScriptedDialogueLifecycleSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    dispatcher_.sink<game::defs::DialogueShowEvent>().connect<&ScriptedDialogueLifecycleSystem::onDialogueShow>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&ScriptedDialogueLifecycleSystem::onDialogueHide>(this);
}

ScriptedDialogueLifecycleSystem::~ScriptedDialogueLifecycleSystem() { dispatcher_.disconnect(this); }

void ScriptedDialogueLifecycleSystem::update(float delta_time) {
    (void)delta_time;

    if (active_dialogues_.empty()) {
        return;
    }

    const entt::entity player = helpers::getPlayerEntity(registry_);
    std::vector<entt::entity> to_close;

    if (player == entt::null) {
        to_close.reserve(active_dialogues_.size());
        for (const auto& entry : active_dialogues_) {
            to_close.push_back(entry.first);
        }
    } else {
        const auto* player_transform = registry_.try_get<engine::component::TransformComponent>(player);
        if (!player_transform) {
            to_close.reserve(active_dialogues_.size());
            for (const auto& entry : active_dialogues_) {
                to_close.push_back(entry.first);
            }
        } else {
            for (const auto& [target, active] : active_dialogues_) {
                if (target == entt::null || !registry_.valid(target)) {
                    to_close.push_back(target);
                    continue;
                }
                const auto* target_transform = registry_.try_get<engine::component::TransformComponent>(target);
                const glm::vec2 target_position = target_transform ? target_transform->position_ : active.show_position;
                if (glm::distance(player_transform->position_, target_position) > active.close_distance) {
                    to_close.push_back(target);
                }
            }
        }
    }

    for (const entt::entity target : to_close) {
        closeDialogue(target);
    }
}

void ScriptedDialogueLifecycleSystem::onDialogueShow(const game::defs::DialogueShowEvent& event) {
    if (event.channel != game::defs::DialogueChannel::Conversation || event.target == entt::null ||
        !helpers::isScriptedInteraction(registry_, event.target)) {
        return;
    }

    const auto* dialogue = registry_.try_get<game::component::DialogueComponent>(event.target);
    const float interact_distance =
        dialogue ? dialogue->interact_distance_ : game::component::DialogueComponent::DEFAULT_INTERACT_DISTANCE;

    if (auto* mutable_dialogue = registry_.try_get<game::component::DialogueComponent>(event.target)) {
        mutable_dialogue->active_ = true;
    }

    active_dialogues_[event.target] = ActiveDialogue{
        .show_position = event.world_position,
        .close_distance =
            std::min(interact_distance * SCRIPTED_DIALOGUE_CLOSE_DISTANCE_FACTOR, SCRIPTED_DIALOGUE_CLOSE_DISTANCE_MAX_PX),
    };
}

void ScriptedDialogueLifecycleSystem::onDialogueHide(const game::defs::DialogueHideEvent& event) {
    if (event.channel != game::defs::DialogueChannel::Conversation) {
        return;
    }

    if (event.target == entt::null) {
        active_dialogues_.clear();
        return;
    }

    active_dialogues_.erase(event.target);
    if (registry_.valid(event.target) &&
        registry_.all_of<game::component::ScriptedInteractionComponent>(event.target)) {
        auto* dialogue = registry_.try_get<game::component::DialogueComponent>(event.target);
        if (!dialogue) {
            return;
        }
        dialogue->active_ = false;
        dialogue->current_line_ = 0;
    }
}

void ScriptedDialogueLifecycleSystem::closeDialogue(const entt::entity target) {
    active_dialogues_.erase(target);
    if (target != entt::null && registry_.valid(target)) {
        auto* dialogue = registry_.try_get<game::component::DialogueComponent>(target);
        if (dialogue) {
            dialogue->active_ = false;
            dialogue->current_line_ = 0;
        }
    }
    helpers::emitDialogueHide(dispatcher_, game::defs::DialogueChannel::Conversation, target);
}

} // namespace game::system
