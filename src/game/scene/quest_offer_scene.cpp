#include "quest_offer_scene.h"

#include "game/data/item_catalog.h"
#include "game/data/quest_data.h"
#include "game/defs/commands.h"

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

[[nodiscard]] std::string findSpeakerName(entt::registry& registry, const entt::entity entity) {
    if (const auto* name = registry.try_get<engine::component::NameComponent>(entity)) {
        return name->name_;
    }
    return "Quest";
}

[[nodiscard]] std::string formatObjectivesText(const game::data::QuestData& quest) {
    std::string text{};
    for (const auto& objective : quest.objectives_) {
        switch (objective.kind_) {
            case game::data::QuestObjectiveKind::DefeatEnemyCount:
                appendSeparated(text, "Defeat " + objective.enemy_id_ + " x" + std::to_string(objective.required_count_));
                break;
        }
    }

    return text.empty() ? "No objectives." : text;
}

[[nodiscard]] std::string resolveRewardItemName(const game::data::ItemCatalog* item_catalog,
                                                const game::data::QuestRewardItemData& item) {
    if (item_catalog) {
        if (const auto* item_data = item_catalog->findItem(item.item_id_hash_)) {
            if (!item_data->display_name_.empty()) {
                return item_data->display_name_;
            }
        }
    }
    return item.item_id_;
}

[[nodiscard]] std::string formatRewardsText(const game::data::QuestData& quest,
                                            const game::data::ItemCatalog* item_catalog) {
    std::string text{};
    if (quest.rewards_.gold_ > 0) {
        appendSeparated(text, "Gold x" + std::to_string(quest.rewards_.gold_));
    }

    for (const auto& item : quest.rewards_.items_) {
        appendSeparated(text, resolveRewardItemName(item_catalog, item) + " x" + std::to_string(item.count_));
    }

    return text.empty() ? "No reward." : text;
}

[[nodiscard]] std::string resolveOfferText(const game::data::QuestData& quest) {
    if (!quest.giver_text_.offer_.empty()) {
        return quest.giver_text_.offer_;
    }
    if (!quest.description_.empty()) {
        return quest.description_;
    }
    return "Will you accept this quest?";
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
    context_.getInputManager().onAction("menu_cancel"_hs).connect<&QuestOfferScene::onMenuCancelPressed>(this);
}

void QuestOfferScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&QuestOfferScene::onMenuCancelPressed>(this);
}

void QuestOfferScene::refreshBindings() {
    speaker_text_ = makeRmlString(findSpeakerName(registry_, giver_));
    offer_text_ = makeRmlString(resolveOfferText(quest_));
    quest_title_ = makeRmlString(quest_.title_.empty() ? quest_.id_ : quest_.title_);
    quest_description_ = makeRmlString(quest_.description_);
    objectives_text_ = makeRmlString(formatObjectivesText(quest_));
    rewards_text_ = makeRmlString(formatRewardsText(quest_, item_catalog_));
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

bool QuestOfferScene::onMenuCancelPressed() {
    onDecline();
    return true;
}

void QuestOfferScene::onAccept() {
    context_.getDispatcher().trigger(game::defs::AcceptQuestCommand{
        .player = player_,
        .giver = giver_,
        .quest_id_hash = quest_.id_hash_,
        .quest_id = quest_.id_});
    requestPopScene();
}

void QuestOfferScene::onDecline() {
    requestPopScene();
}

} // namespace game::scene
