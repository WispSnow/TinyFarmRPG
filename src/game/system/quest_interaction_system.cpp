#include "game/system/quest_interaction_system.h"

#include "game/component/npc_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/defs/commands.h"
#include "game/domain/quest_log_ops.h"

#include "engine/component/name_component.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>

namespace game::system {
namespace {

constexpr std::uint8_t NOTIFICATION_CHANNEL = 1;
constexpr float NOTIFICATION_SECONDS = 2.0f;

[[nodiscard]] std::string findSpeakerName(entt::registry& registry, const entt::entity entity) {
    if (const auto* name = registry.try_get<engine::component::NameComponent>(entity)) {
        return name->name_;
    }
    return {};
}

[[nodiscard]] const std::string& selectQuestText(const game::data::QuestData& quest,
                                                 const QuestInteractionSystem::InteractionState state) {
    switch (state) {
        case QuestInteractionSystem::InteractionState::Offerable:
            return quest.giver_text_.offer_;
        case QuestInteractionSystem::InteractionState::InProgress:
            return quest.giver_text_.progress_;
        case QuestInteractionSystem::InteractionState::ReadyToTurnIn:
            return quest.giver_text_.ready_to_turn_in_;
        case QuestInteractionSystem::InteractionState::Completed:
            return quest.giver_text_.completed_;
    }

    return quest.giver_text_.progress_;
}

[[nodiscard]] std::string makeFallbackQuestText(const game::data::QuestData& quest,
                                                const QuestInteractionSystem::InteractionState state) {
    switch (state) {
        case QuestInteractionSystem::InteractionState::Offerable:
            return "接取任务：" + quest.title_;
        case QuestInteractionSystem::InteractionState::InProgress:
            return "任务进行中：" + quest.title_;
        case QuestInteractionSystem::InteractionState::ReadyToTurnIn:
            return "可以交付任务：" + quest.title_;
        case QuestInteractionSystem::InteractionState::Completed:
            return "任务已完成：" + quest.title_;
    }

    return quest.title_;
}

} // namespace

QuestInteractionSystem::QuestInteractionSystem(entt::registry& registry,
                                               entt::dispatcher& dispatcher,
                                               const game::data::QuestCatalog& quest_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      quest_catalog_(quest_catalog) {
    dispatcher_.sink<game::defs::InteractCommand>().connect<&QuestInteractionSystem::onInteractCommand>(this);
}

QuestInteractionSystem::~QuestInteractionSystem() {
    dispatcher_.disconnect(this);
}

void QuestInteractionSystem::update(const float delta_time) {
    helpers::updateTimedNotification(registry_, dispatcher_, NOTIFICATION_CHANNEL, notification_, delta_time);
}

QuestInteractionSystem::InteractionState QuestInteractionSystem::resolveState(
    const game::component::QuestLogComponent& quest_log,
    const game::data::QuestData& quest) const {
    if (game::domain::quest_log_ops::isQuestCompleted(quest_log, quest.id_hash_)) {
        return InteractionState::Completed;
    }
    if (game::domain::quest_log_ops::isQuestReadyToTurnIn(quest_log, quest)) {
        return InteractionState::ReadyToTurnIn;
    }
    if (game::domain::quest_log_ops::isQuestActive(quest_log, quest.id_hash_)) {
        return InteractionState::InProgress;
    }
    return InteractionState::Offerable;
}

void QuestInteractionSystem::showQuestText(const entt::entity giver,
                                           const game::data::QuestData& quest,
                                           const InteractionState state) {
    std::string text = selectQuestText(quest, state);
    if (text.empty()) {
        text = makeFallbackQuestText(quest, state);
    }

    helpers::showTimedNotification(
        registry_,
        dispatcher_,
        NOTIFICATION_CHANNEL,
        notification_,
        giver,
        findSpeakerName(registry_, giver),
        std::move(text),
        NOTIFICATION_SECONDS);
}

void QuestInteractionSystem::onInteractCommand(const game::defs::InteractCommand& event) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || event.player != player) {
        return;
    }
    if (event.target == entt::null || !registry_.valid(event.target)) {
        return;
    }

    const auto* giver = registry_.try_get<game::component::QuestGiverComponent>(event.target);
    if (!giver || giver->quest_id_hash_ == entt::null) {
        return;
    }

    auto* quest_log = registry_.try_get<game::component::QuestLogComponent>(player);
    if (!quest_log) {
        spdlog::warn("QuestInteractionSystem: 玩家缺少 QuestLogComponent，忽略任务交互。");
        return;
    }

    const auto* quest = quest_catalog_.findQuest(giver->quest_id_hash_);
    if (!quest) {
        spdlog::warn("QuestInteractionSystem: giver quest_id='{}' 未在 QuestCatalog 中找到。", giver->quest_id_);
        return;
    }

    const InteractionState state = resolveState(*quest_log, *quest);
    if (state == InteractionState::Offerable) {
        if (!game::domain::quest_log_ops::tryAcceptQuest(*quest_log, *quest)) {
            return;
        }
    }

    showQuestText(event.target, *quest, state);
}

} // namespace game::system
