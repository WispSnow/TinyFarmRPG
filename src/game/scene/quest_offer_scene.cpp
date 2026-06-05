#include "quest_offer_scene.h"

#include "game/data/item_catalog.h"
#include "game/data/quest_data.h"
#include "game/defs/commands.h"
#include "game/defs/options_events.h"
#include "game/runtime/service_lookup.h"
#include "game/ui/localized_text.h"

#include "engine/component/name_component.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/quest_offer.rml";
constexpr std::string_view MODEL_NAME = "quest_offer";

[[nodiscard]] Rml::String makeRmlString(const std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

void appendSeparated(std::string& text, const std::string_view part) {
    if (part.empty()) {
        return;
    }
    if (!text.empty()) {
        text.append(" / ");
    }
    text.append(part);
}

[[nodiscard]] std::string findSpeakerName(entt::registry& registry,
                                          const entt::entity entity,
                                          const game::runtime::LocalizationService* localization) {
    if (const auto* name = registry.try_get<engine::component::NameComponent>(entity)) {
        return name->name_;
    }
    return game::ui::localizeTextOrFallback(localization, "quest_offer.speaker.default", "Quest");
}

[[nodiscard]] std::string formatObjectivesText(const game::data::QuestData& quest,
                                               const game::runtime::LocalizationService* localization) {
    std::string text{};
    for (const auto& objective : quest.objectives_) {
        switch (objective.kind_) {
            case game::data::QuestObjectiveKind::DefeatEnemyCount: {
                const std::string enemy_name = game::ui::localizeIdName(localization, objective.enemy_id_);
                appendSeparated(
                    text,
                    game::ui::formatTextOrFallback(
                        localization,
                        "quest_offer.objective.defeat_enemy_count",
                        {{"enemy", enemy_name}, {"count", std::to_string(objective.required_count_)}},
                        [&objective, &enemy_name] {
                            return "Defeat " + enemy_name + " x" +
                                   std::to_string(objective.required_count_);
                        }));
                break;
            }
        }
    }

    return text.empty()
        ? game::ui::localizeTextOrFallback(localization, "quest_offer.objective.none", "No objectives.")
        : text;
}

[[nodiscard]] std::string resolveRewardItemName(const game::data::ItemCatalog* item_catalog,
                                                const game::data::QuestRewardItemData& item,
                                                const game::runtime::LocalizationService* localization) {
    if (item_catalog) {
        if (const auto* item_data = item_catalog->findItem(item.item_id_hash_)) {
            if (!item_data->display_name_.empty()) {
                return game::ui::tryLocalize(localization, item_data->display_name_);
            }
        }
    }
    return item.item_id_;
}

[[nodiscard]] std::string formatRewardsText(const game::data::QuestData& quest,
                                            const game::data::ItemCatalog* item_catalog,
                                            const game::runtime::LocalizationService* localization) {
    std::string text{};
    if (quest.rewards_.gold_ > 0) {
        appendSeparated(
            text,
            game::ui::formatTextOrFallback(
                localization,
                "quest_offer.reward.gold",
                {{"amount", std::to_string(quest.rewards_.gold_)}},
                [&quest] { return "Gold x" + std::to_string(quest.rewards_.gold_); }));
    }

    for (const auto& item : quest.rewards_.items_) {
        const std::string item_name = resolveRewardItemName(item_catalog, item, localization);
        appendSeparated(
            text,
            game::ui::formatTextOrFallback(
                localization,
                "quest_offer.reward.item",
                {{"item", item_name}, {"count", std::to_string(item.count_)}},
                [&item, &item_name] { return item_name + " x" + std::to_string(item.count_); }));
    }

    return text.empty() ? game::ui::localizeTextOrFallback(localization, "quest_offer.reward.none", "No reward.") : text;
}

[[nodiscard]] std::string resolveOfferText(const game::data::QuestData& quest,
                                           const game::runtime::LocalizationService* localization) {
    if (!quest.giver_text_.offer_.empty()) {
        return game::ui::tryLocalize(localization, quest.giver_text_.offer_);
    }
    if (!quest.description_.empty()) {
        return game::ui::tryLocalize(localization, quest.description_);
    }
    return game::ui::localizeTextOrFallback(
        localization,
        "quest_offer.default_offer",
        "Will you accept this quest?");
}

} // namespace

namespace game::scene {

using namespace entt::literals;

QuestOfferScene::QuestOfferScene(std::string_view name,
                                 engine::core::Context& context,
                                 entt::registry& registry,
                                 const entt::entity player,
                                 const entt::entity giver,
                                 const game::data::QuestData& quest,
                                 const game::data::ItemCatalog* item_catalog)
    : engine::scene::Scene(name, context),
      registry_(registry),
      player_(player),
      giver_(giver),
      quest_(quest),
      item_catalog_(item_catalog),
      previous_state_(context.getGameState().getCurrentState()) {
}

QuestOfferScene::~QuestOfferScene() {
    disconnectRuntimeListeners();
    shutdownUI();
}

bool QuestOfferScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Dialogue);
    context_pushed_ = true;
    localization_ = game::runtime::findLocalizationService(registry_);

    refreshBindings();
    if (!initUI()) {
        return false;
    }

    connectRuntimeListeners();
    if (!Scene::init()) {
        return false;
    }

    return true;
}

void QuestOfferScene::clean() {
    shutdownUI();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

engine::scene::SceneUiCoverage QuestOfferScene::uiCoverage() const {
    return engine::scene::SceneUiCoverage::HideUnderlyingSceneUi;
}

bool QuestOfferScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("QuestOfferScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME);
    if (!constructor) {
        spdlog::error("QuestOfferScene: 创建 data model 失败。");
        return false;
    }

    constructor.Bind("speaker_text", &speaker_text_);
    constructor.Bind("offer_text", &offer_text_);
    constructor.Bind("quest_title", &quest_title_);
    constructor.Bind("quest_description", &quest_description_);
    constructor.Bind("objectives_text", &objectives_text_);
    constructor.Bind("rewards_text", &rewards_text_);

    if (!document_controller_.bindSimpleEvent(constructor, "accept", [this] { onAccept(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "decline", [this] { onDecline(); })) {
        spdlog::error("QuestOfferScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("QuestOfferScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    focusDefaultAction();
    return true;
}

void QuestOfferScene::shutdownUI() {
    document_controller_.unload();
}

void QuestOfferScene::connectRuntimeListeners() {
    context_.getInputManager().onAction("menu_confirm"_hs).connect<&QuestOfferScene::onMenuConfirmPressed>(this);
    context_.getInputManager().onAction("menu_cancel"_hs).connect<&QuestOfferScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().connect<&QuestOfferScene::onLanguageChanged>(this);
    spdlog::info("QuestOfferScene: opened quest_id='{}'.", quest_.id_);
}

void QuestOfferScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_confirm"_hs).disconnect<&QuestOfferScene::onMenuConfirmPressed>(this);
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&QuestOfferScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().disconnect<&QuestOfferScene::onLanguageChanged>(this);
}

void QuestOfferScene::refreshBindings() {
    speaker_text_ = makeRmlString(findSpeakerName(registry_, giver_, localization_));
    offer_text_ = makeRmlString(resolveOfferText(quest_, localization_));
    quest_title_ = makeRmlString(
        quest_.title_.empty() ? quest_.id_ : game::ui::tryLocalize(localization_, quest_.title_));
    quest_description_ = makeRmlString(game::ui::tryLocalize(localization_, quest_.description_));
    objectives_text_ = makeRmlString(formatObjectivesText(quest_, localization_));
    rewards_text_ = makeRmlString(formatRewardsText(quest_, item_catalog_, localization_));
}

void QuestOfferScene::focusDefaultAction() {
    auto* document = document_controller_.document();
    if (!document) {
        return;
    }

    auto* accept_button = document->GetElementById("quest-offer-accept-button");
    if (accept_button) {
        accept_button->Focus(true);
    }
}

bool QuestOfferScene::onMenuConfirmPressed() {
    onAccept();
    return true;
}

bool QuestOfferScene::onMenuCancelPressed() {
    onDecline();
    return true;
}

void QuestOfferScene::onLanguageChanged(const game::defs::LanguageChangedEvent&) {
    refreshBindings();
    document_controller_.markDirty("speaker_text");
    document_controller_.markDirty("offer_text");
    document_controller_.markDirty("quest_title");
    document_controller_.markDirty("quest_description");
    document_controller_.markDirty("objectives_text");
    document_controller_.markDirty("rewards_text");
}

void QuestOfferScene::onAccept() {
    if (resolved_) {
        return;
    }
    resolved_ = true;
    spdlog::info("QuestOfferScene: accepted quest_id='{}'.", quest_.id_);
    context_.getDispatcher().trigger(game::defs::AcceptQuestCommand{
        .player = player_,
        .giver = giver_,
        .quest_id_hash = quest_.id_hash_,
        .quest_id = quest_.id_});
    requestPopScene();
}

void QuestOfferScene::onDecline() {
    if (resolved_) {
        return;
    }
    resolved_ = true;
    requestPopScene();
}

} // namespace game::scene
