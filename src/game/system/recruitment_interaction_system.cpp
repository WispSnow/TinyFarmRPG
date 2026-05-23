#include "recruitment_interaction_system.h"

#include "game/component/merchant_component.h"
#include "game/component/npc_component.h"
#include "game/component/party_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/defs/party_ids.h"

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <limits>
#include <string_view>

namespace {

constexpr game::defs::DialogueChannel DIALOGUE_CHANNEL = game::defs::DialogueChannel::Conversation;
constexpr game::defs::DialogueChannel NOTIFICATION_CHANNEL = game::defs::DialogueChannel::Notice;
constexpr float NOTIFICATION_SECONDS = 2.0f;
constexpr float DIALOGUE_CLOSE_DISTANCE_FACTOR = 1.4f;

[[nodiscard]] bool containsString(const std::vector<std::string>& values, std::string_view value) {
    return std::any_of(values.begin(), values.end(), [value](const std::string& current) {
        return current == value;
    });
}

[[nodiscard]] std::string findSpeakerName(entt::registry& registry, const entt::entity entity) {
    if (const auto* name = registry.try_get<engine::component::NameComponent>(entity)) {
        return name->name_;
    }
    return {};
}

} // namespace

namespace game::system {

RecruitmentInteractionSystem::RecruitmentInteractionSystem(entt::registry& registry,
                                                           entt::dispatcher& dispatcher,
                                                           const game::data::RpgCatalog& rpg_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      rpg_catalog_(rpg_catalog) {
    dispatcher_.sink<game::defs::InteractCommand>().connect<&RecruitmentInteractionSystem::onInteractCommand>(this);
}

RecruitmentInteractionSystem::~RecruitmentInteractionSystem() {
    dispatcher_.disconnect(this);
}

bool RecruitmentInteractionSystem::loadDialogueFile(std::string_view file_path) {
    std::ifstream file(std::string{file_path});
    if (!file.is_open()) {
        spdlog::warn("RecruitmentInteractionSystem: 无法打开对话文件 {}", file_path);
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;
        for (auto& [key, value] : json.items()) {
            if (!value.is_array()) {
                continue;
            }
            std::vector<std::string> lines;
            lines.reserve(value.size());
            for (const auto& line : value) {
                if (line.is_string()) {
                    lines.push_back(line.get<std::string>());
                }
            }
            dialogue_table_.emplace(entt::hashed_string(key.c_str()), std::move(lines));
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("RecruitmentInteractionSystem: 解析对话文件失败 {} - {}", file_path, e.what());
        return false;
    }
}

void RecruitmentInteractionSystem::update(const float delta_time) {
    helpers::updateTimedNotification(registry_, dispatcher_, NOTIFICATION_CHANNEL, notification_, delta_time);

    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        if (active_entity_ != entt::null) {
            closeDialogue(active_entity_);
            active_entity_ = entt::null;
        }
        return;
    }

    const auto* player_transform = registry_.try_get<engine::component::TransformComponent>(player);
    if (!player_transform) {
        return;
    }

    if (active_entity_ != entt::null) {
        auto* dialogue = registry_.try_get<game::component::DialogueComponent>(active_entity_);
        auto* transform = registry_.try_get<engine::component::TransformComponent>(active_entity_);
        if (!dialogue || !transform ||
            glm::distance(transform->position_, player_transform->position_) >
                dialogue->interact_distance_ * DIALOGUE_CLOSE_DISTANCE_FACTOR) {
            closeDialogue(active_entity_);
            active_entity_ = entt::null;
            return;
        }

    }
}

void RecruitmentInteractionSystem::onInteractCommand(const game::defs::InteractCommand& event) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || event.player != player) {
        return;
    }
    if (event.target == entt::null || !registry_.valid(event.target)) {
        return;
    }
    if (registry_.all_of<game::component::MerchantComponent>(event.target) ||
        registry_.all_of<game::component::QuestGiverComponent>(event.target)) {
        return;
    }

    const auto* recruitable = registry_.try_get<game::component::RecruitableComponent>(event.target);
    if (!recruitable || recruitable->actor_id_hash_ == entt::null || recruitable->actor_id_.empty()) {
        return;
    }

    const auto* actor = rpg_catalog_.findActor(recruitable->actor_id_hash_);
    if (!actor) {
        spdlog::warn("RecruitmentInteractionSystem: recruit_actor_id='{}' 未在 RpgCatalog 中找到。", recruitable->actor_id_);
        return;
    }

    if (isRecruited(recruitable->actor_id_)) {
        const std::string name = actor->display_name_.empty() ? actor->id_ : actor->display_name_;
        showNotification(event.target, name + " is already in the party.");
        return;
    }

    auto* dialogue = registry_.try_get<game::component::DialogueComponent>(event.target);
    if (!dialogue || dialogue->cooldown_timer_ > 0.0f) {
        return;
    }

    std::vector<std::string> fallback_lines{};
    const std::vector<std::string>* lines = nullptr;
    const auto lines_it = dialogue_table_.find(dialogue->dialogue_id_);
    if (lines_it != dialogue_table_.end() && !lines_it->second.empty()) {
        lines = &lines_it->second;
    } else {
        fallback_lines.push_back("Let me join you.");
        lines = &fallback_lines;
    }

    if (active_entity_ != event.target) {
        startDialogue(event.target, *dialogue, *lines);
        return;
    }

    if (advanceDialogue(event.target, *dialogue, *lines)) {
        return;
    }

    endDialogueAndRequestJoin(player, event.target, *dialogue);
}

bool RecruitmentInteractionSystem::isRecruited(std::string_view actor_id) const {
    auto players = registry_.view<game::component::PlayerTag, game::component::PartyComponent>();
    if (players.begin() == players.end()) {
        return actor_id == game::defs::kDefaultPlayerActorId;
    }
    const auto& party = players.get<game::component::PartyComponent>(*players.begin());
    return containsString(party.recruited_actor_ids_, actor_id);
}

void RecruitmentInteractionSystem::showNotification(entt::entity target, std::string text) {
    helpers::showTimedNotification(
        registry_,
        dispatcher_,
        NOTIFICATION_CHANNEL,
        notification_,
        target,
        findSpeakerName(registry_, target),
        std::move(text),
        NOTIFICATION_SECONDS);
}

void RecruitmentInteractionSystem::startDialogue(entt::entity entity,
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

bool RecruitmentInteractionSystem::advanceDialogue(entt::entity entity,
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

void RecruitmentInteractionSystem::endDialogueAndRequestJoin(entt::entity player,
                                                             entt::entity entity,
                                                             game::component::DialogueComponent& dialogue) {
    const auto* recruitable = registry_.try_get<game::component::RecruitableComponent>(entity);
    closeDialogue(entity);
    if (active_entity_ == entity) {
        active_entity_ = entt::null;
    }
    if (!recruitable) {
        return;
    }

    dispatcher_.trigger(game::defs::RecruitOfferRequestedEvent{
        .player = player,
        .recruiter = entity,
        .actor_id_hash = recruitable->actor_id_hash_,
        .actor_id = recruitable->actor_id_});
    (void)dialogue;
}

void RecruitmentInteractionSystem::closeDialogue(entt::entity entity) {
    if (auto* dialogue = registry_.try_get<game::component::DialogueComponent>(entity)) {
        dialogue->active_ = false;
        dialogue->current_line_ = 0;
    }
    helpers::emitDialogueHide(dispatcher_, DIALOGUE_CHANNEL, entity);
}

void RecruitmentInteractionSystem::showLine(entt::entity entity,
                                            const std::vector<std::string>& lines,
                                            std::size_t line_index) {
    if (line_index >= lines.size()) {
        return;
    }
    entt::id_type speaker_actor_id_hash = entt::null;
    std::string speaker_actor_id{};
    if (const auto* recruitable = registry_.try_get<game::component::RecruitableComponent>(entity)) {
        speaker_actor_id_hash = recruitable->actor_id_hash_;
        speaker_actor_id = recruitable->actor_id_;
    }
    helpers::emitDialogueShow(
        dispatcher_,
        DIALOGUE_CHANNEL,
        entity,
        findSpeakerName(registry_, entity),
        lines[line_index],
        {},
        speaker_actor_id_hash,
        std::move(speaker_actor_id));
    registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
    if (auto* state = registry_.try_get<game::component::StateComponent>(entity)) {
        state->action_ = game::component::Action::Idle;
    }
}

} // namespace game::system
