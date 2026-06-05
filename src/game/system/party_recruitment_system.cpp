#include "party_recruitment_system.h"

#include "game/component/party_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/party_ids.h"
#include "game/domain/actor_progression_service.h"
#include "game/runtime/service_lookup.h"
#include "game/ui/localized_text.h"

#include "engine/component/name_component.h"
#include "engine/spatial/spatial_index_manager.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <string>
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

void ensureDefaultPartyActor(game::component::PartyComponent& party) {
    if (!containsString(party.recruited_actor_ids_, game::defs::kDefaultPlayerActorId)) {
        party.recruited_actor_ids_.insert(
            party.recruited_actor_ids_.begin(),
            std::string(game::defs::kDefaultPlayerActorId));
    }
    if (!containsString(party.active_actor_ids_, game::defs::kDefaultPlayerActorId)) {
        party.active_actor_ids_.insert(
            party.active_actor_ids_.begin(),
            std::string(game::defs::kDefaultPlayerActorId));
    }
}

[[nodiscard]] std::string findSpeakerName(entt::registry& registry, const entt::entity entity) {
    if (const auto* name = registry.try_get<engine::component::NameComponent>(entity)) {
        return name->name_;
    }
    return {};
}

} // namespace

namespace game::system {

PartyRecruitmentSystem::PartyRecruitmentSystem(entt::registry& registry,
                                               entt::dispatcher& dispatcher,
                                               const game::data::RpgCatalog& rpg_catalog,
                                               engine::spatial::SpatialIndexManager* spatial_index_manager)
    : registry_(registry),
      dispatcher_(dispatcher),
      rpg_catalog_(rpg_catalog),
      spatial_index_manager_(spatial_index_manager) {
    dispatcher_.sink<game::defs::RecruitPartyMemberCommand>()
        .connect<&PartyRecruitmentSystem::onRecruitPartyMemberCommand>(this);
}

PartyRecruitmentSystem::~PartyRecruitmentSystem() {
    dispatcher_.disconnect(this);
}

void PartyRecruitmentSystem::update(const float delta_time) {
    helpers::updateTimedNotification(registry_, dispatcher_, NOTIFICATION_CHANNEL, notification_, delta_time);
}

void PartyRecruitmentSystem::onRecruitPartyMemberCommand(const game::defs::RecruitPartyMemberCommand& command) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || command.player != player) {
        return;
    }
    if (command.actor_id.empty() || command.actor_id_hash == entt::null) {
        return;
    }

    if (command.recruiter != entt::null && registry_.valid(command.recruiter)) {
        const auto* recruitable = registry_.try_get<game::component::RecruitableComponent>(command.recruiter);
        if (!recruitable || recruitable->actor_id_hash_ != command.actor_id_hash) {
            return;
        }
    }

    const auto* actor = rpg_catalog_.findActor(command.actor_id_hash);
    if (!actor) {
        spdlog::warn("PartyRecruitmentSystem: actor_id='{}' 未在 RpgCatalog 中找到。", command.actor_id);
        return;
    }

    auto& party = registry_.get_or_emplace<game::component::PartyComponent>(player);
    ensureDefaultPartyActor(party);

    const auto* localization = game::runtime::findLocalizationService(registry_);
    const std::string display_name = actor->display_name_.empty()
                                         ? actor->id_
                                         : game::ui::tryLocalize(localization, actor->display_name_);
    if (containsString(party.recruited_actor_ids_, actor->id_)) {
        showNotification(
            command.recruiter,
            game::ui::formatTextOrFallback(
                localization,
                "recruitment.already_in_party",
                {{"actor", display_name}},
                [&display_name] { return display_name + " is already in the party."; }));
        return;
    }

    party.recruited_actor_ids_.push_back(actor->id_);
    if (!containsString(party.active_actor_ids_, actor->id_) &&
        party.active_actor_ids_.size() < party.max_active_members_) {
        party.active_actor_ids_.push_back(actor->id_);
    }

    auto& runtime_stats = registry_.get_or_emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime_stats.states_by_actor_id_.try_emplace(
        actor->id_,
        game::domain::ActorProgressionService::initialState(rpg_catalog_, *actor, nullptr));
    ++runtime_stats.revision_;
    spdlog::info("PartyRecruitmentSystem: recruited actor_id='{}'.", actor->id_);

    showNotification(
        player,
        game::ui::formatTextOrFallback(
            localization,
            "recruitment.joined_party",
            {{"actor", display_name}},
            [&display_name] { return display_name + " joined the party."; }));
    removeRecruiterFromMap(command.recruiter);
}

void PartyRecruitmentSystem::removeRecruiterFromMap(const entt::entity recruiter) {
    if (recruiter == entt::null || !registry_.valid(recruiter)) {
        return;
    }
    if (!registry_.all_of<game::component::RecruitableComponent>(recruiter)) {
        return;
    }

    helpers::emitDialogueHide(dispatcher_, game::defs::DialogueChannel::Conversation, recruiter);
    helpers::emitDialogueHide(dispatcher_, NOTIFICATION_CHANNEL, recruiter);
    if (spatial_index_manager_ && spatial_index_manager_->isInitialized()) {
        spatial_index_manager_->removeColliderEntity(recruiter);
    }
    registry_.destroy(recruiter);
}

void PartyRecruitmentSystem::showNotification(entt::entity target, std::string text) {
    if (target == entt::null || !registry_.valid(target)) {
        target = helpers::getPlayerEntity(registry_);
    }
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
