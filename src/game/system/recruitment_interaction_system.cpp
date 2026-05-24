#include "recruitment_interaction_system.h"

#include "game/component/merchant_component.h"
#include "game/component/party_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/defs/party_ids.h"
#include "game/system/system_helpers.h"

#include "engine/component/name_component.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string_view>
#include <vector>

namespace {

constexpr game::defs::DialogueChannel NOTIFICATION_CHANNEL = game::defs::DialogueChannel::Notice;
constexpr float NOTIFICATION_SECONDS = 2.0f;

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

void RecruitmentInteractionSystem::update(const float delta_time) {
    helpers::updateTimedNotification(registry_, dispatcher_, NOTIFICATION_CHANNEL, notification_, delta_time);
}

void RecruitmentInteractionSystem::onInteractCommand(const game::defs::InteractCommand& event) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || event.player != player) {
        return;
    }
    if (event.target == entt::null || !registry_.valid(event.target)) {
        return;
    }
    if (game::system::helpers::isScriptedInteraction(registry_, event.target)) {
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

    // Fallback for non-scripted recruitables. Authored recruitment presentation
    // should live in Lua and mark the map actor with ScriptedInteractionComponent.
    dispatcher_.trigger(game::defs::RecruitOfferRequestedEvent{
        .player = player,
        .recruiter = event.target,
        .actor_id_hash = recruitable->actor_id_hash_,
        .actor_id = recruitable->actor_id_});
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

} // namespace game::system
