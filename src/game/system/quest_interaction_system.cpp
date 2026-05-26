#include "game/system/quest_interaction_system.h"

#include "game/component/npc_component.h"
#include "game/component/merchant_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/quest_log_ops.h"
#include "game/domain/quest_turn_in_service.h"
#include "game/runtime/service_lookup.h"
#include "game/ui/localized_text.h"

#include "engine/component/name_component.h"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>

namespace game::system {
namespace {

constexpr game::defs::DialogueChannel NOTIFICATION_CHANNEL = game::defs::DialogueChannel::Notice;
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
                                                const QuestInteractionSystem::InteractionState state,
                                                const game::runtime::LocalizationService* localization) {
    const std::string title = quest.title_.empty() ? quest.id_ : game::ui::tryLocalize(localization, quest.title_);
    switch (state) {
        case QuestInteractionSystem::InteractionState::Offerable:
            return game::ui::formatTextOrFallback(
                localization,
                "quest_interaction.accept",
                {{"quest", title}},
                [&title] { return "Accept Quest: " + title; });
        case QuestInteractionSystem::InteractionState::InProgress:
            return game::ui::formatTextOrFallback(
                localization,
                "quest_interaction.in_progress",
                {{"quest", title}},
                [&title] { return "Quest In Progress: " + title; });
        case QuestInteractionSystem::InteractionState::ReadyToTurnIn:
            return game::ui::formatTextOrFallback(
                localization,
                "quest_interaction.ready",
                {{"quest", title}},
                [&title] { return "Ready to Turn In: " + title; });
        case QuestInteractionSystem::InteractionState::Completed:
            return game::ui::formatTextOrFallback(
                localization,
                "quest_interaction.completed",
                {{"quest", title}},
                [&title] { return "Quest Completed: " + title; });
    }

    return title;
}

void appendLine(std::string& text, const std::string& line) {
    if (line.empty()) {
        return;
    }
    if (!text.empty()) {
        text.append("\n");
    }
    text.append(line);
}

[[nodiscard]] std::string formatTurnInSuccessText(const game::data::QuestData& quest,
                                                  const game::domain::QuestTurnInResult& result,
                                                  const game::runtime::LocalizationService* localization) {
    std::string text = game::ui::tryLocalize(localization, quest.giver_text_.completed_);
    if (text.empty()) {
        text = makeFallbackQuestText(quest, QuestInteractionSystem::InteractionState::Completed, localization);
    }

    if (result.gold_reward > 0) {
        appendLine(
            text,
            game::ui::formatTextOrFallback(
                localization,
                "reward.gold_gained",
                {{"amount", std::to_string(result.gold_reward)}},
                [&result] { return fmt::format("Gained Gold {}", result.gold_reward); }));
    }
    for (const auto& item_reward : result.item_rewards) {
        const std::string item_name = game::ui::tryLocalize(localization, item_reward.item_name);
        appendLine(
            text,
            game::ui::formatTextOrFallback(
                localization,
                "reward.item_gained",
                {{"item", item_name}, {"count", std::to_string(item_reward.count)}},
                [&item_name, &item_reward] { return fmt::format("Gained {} x{}", item_name, item_reward.count); }));
    }

    return text;
}

} // namespace

QuestInteractionSystem::QuestInteractionSystem(entt::registry& registry,
                                               entt::dispatcher& dispatcher,
                                               const game::data::QuestCatalog& quest_catalog,
                                               game::domain::QuestTurnInService& quest_turn_in_service)
    : registry_(registry),
      dispatcher_(dispatcher),
      quest_catalog_(quest_catalog),
      quest_turn_in_service_(quest_turn_in_service) {
    dispatcher_.sink<game::defs::InteractCommand>().connect<&QuestInteractionSystem::onInteractCommand>(this);
    dispatcher_.sink<game::defs::AcceptQuestCommand>().connect<&QuestInteractionSystem::onAcceptQuestCommand>(this);
    dispatcher_.sink<game::defs::TurnInQuestCommand>().connect<&QuestInteractionSystem::onTurnInQuestCommand>(this);
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

void QuestInteractionSystem::showText(const entt::entity giver, std::string text) {
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

void QuestInteractionSystem::showQuestText(const entt::entity giver,
                                           const game::data::QuestData& quest,
                                           const InteractionState state) {
    std::string text = selectQuestText(quest, state);
    const auto* localization = game::runtime::findLocalizationService(registry_);
    text = game::ui::tryLocalize(localization, text);
    if (text.empty()) {
        text = makeFallbackQuestText(quest, state, localization);
    }
    showText(giver, std::move(text));
}

void QuestInteractionSystem::onInteractCommand(const game::defs::InteractCommand& event) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || event.player != player) {
        return;
    }
    if (event.target == entt::null || !registry_.valid(event.target)) {
        return;
    }
    if (helpers::isScriptedInteraction(registry_, event.target)) {
        return;
    }
    if (registry_.all_of<game::component::MerchantComponent>(event.target)) {
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
    switch (state) {
        case InteractionState::Offerable:
            dispatcher_.trigger(game::defs::QuestOfferRequestedEvent{
                .player = player,
                .giver = event.target,
                .quest_id_hash = quest->id_hash_,
                .quest_id = quest->id_});
            return;
        case InteractionState::ReadyToTurnIn:
            turnInQuest(player, event.target, *quest, *quest_log);
            return;
        case InteractionState::InProgress:
        case InteractionState::Completed:
            showQuestText(event.target, *quest, state);
            return;
    }
}

void QuestInteractionSystem::onAcceptQuestCommand(const game::defs::AcceptQuestCommand& command) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || command.player != player) {
        return;
    }
    if (command.giver == entt::null || !registry_.valid(command.giver)) {
        return;
    }
    if (registry_.all_of<game::component::MerchantComponent>(command.giver)) {
        return;
    }

    const auto* giver = registry_.try_get<game::component::QuestGiverComponent>(command.giver);
    if (!giver || giver->quest_id_hash_ == entt::null) {
        return;
    }
    if (command.quest_id_hash == entt::null || command.quest_id_hash != giver->quest_id_hash_) {
        return;
    }

    auto* quest_log = registry_.try_get<game::component::QuestLogComponent>(player);
    if (!quest_log) {
        spdlog::warn("QuestInteractionSystem: 玩家缺少 QuestLogComponent，忽略任务接受。");
        return;
    }

    const auto* quest = quest_catalog_.findQuest(giver->quest_id_hash_);
    if (!quest) {
        spdlog::warn("QuestInteractionSystem: giver quest_id='{}' 未在 QuestCatalog 中找到。", giver->quest_id_);
        return;
    }

    if (resolveState(*quest_log, *quest) != InteractionState::Offerable) {
        return;
    }

    if (!game::domain::quest_log_ops::tryAcceptQuest(*quest_log, *quest)) {
        return;
    }
    dispatcher_.trigger(game::defs::QuestAcceptedEvent{
        .player = player,
        .giver = command.giver,
        .quest_id_hash = quest->id_hash_,
        .quest_id = quest->id_,
    });
    if (!helpers::isScriptedInteraction(registry_, command.giver)) {
        showQuestText(command.giver, *quest, InteractionState::Offerable);
    }
}

void QuestInteractionSystem::onTurnInQuestCommand(const game::defs::TurnInQuestCommand& command) {
    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null || command.player != player) {
        return;
    }
    if (command.giver == entt::null || !registry_.valid(command.giver)) {
        return;
    }
    if (registry_.all_of<game::component::MerchantComponent>(command.giver)) {
        return;
    }

    const auto* giver = registry_.try_get<game::component::QuestGiverComponent>(command.giver);
    if (!giver || giver->quest_id_hash_ == entt::null) {
        return;
    }
    if (command.quest_id_hash == entt::null || command.quest_id_hash != giver->quest_id_hash_) {
        return;
    }

    auto* quest_log = registry_.try_get<game::component::QuestLogComponent>(player);
    if (!quest_log) {
        spdlog::warn("QuestInteractionSystem: 玩家缺少 QuestLogComponent，忽略任务交付。");
        return;
    }

    const auto* quest = quest_catalog_.findQuest(giver->quest_id_hash_);
    if (!quest) {
        spdlog::warn("QuestInteractionSystem: giver quest_id='{}' 未在 QuestCatalog 中找到。", giver->quest_id_);
        return;
    }

    if (resolveState(*quest_log, *quest) != InteractionState::ReadyToTurnIn) {
        return;
    }

    turnInQuest(player, command.giver, *quest, *quest_log);
}

void QuestInteractionSystem::turnInQuest(const entt::entity player,
                                         const entt::entity giver,
                                         const game::data::QuestData& quest,
                                         game::component::QuestLogComponent& quest_log) {
    const auto turn_in_result = quest_turn_in_service_.turnIn(player, quest, quest_log);
    if (!turn_in_result.completed()) {
        showText(giver, turn_in_result.failure_message);
        return;
    }
    dispatcher_.trigger(game::defs::QuestCompletedEvent{
        .player = player,
        .giver = giver,
        .quest_id_hash = quest.id_hash_,
        .quest_id = quest.id_,
    });
    if (!helpers::isScriptedInteraction(registry_, giver)) {
        showText(giver, formatTurnInSuccessText(quest, turn_in_result, game::runtime::findLocalizationService(registry_)));
    }
}

} // namespace game::system
