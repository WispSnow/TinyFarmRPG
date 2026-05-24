#include "recruit_offer_scene.h"

#include "game/data/rpg_data.h"
#include "game/defs/commands.h"
#include "game/defs/party_ids.h"

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

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/recruit_offer.rml";
constexpr std::string_view MODEL_NAME = "recruit_offer";

[[nodiscard]] Rml::String makeRmlString(const std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

[[nodiscard]] std::string findSpeakerName(entt::registry& registry, const entt::entity entity) {
    if (const auto* name = registry.try_get<engine::component::NameComponent>(entity)) {
        return name->name_;
    }
    return "Recruit";
}

[[nodiscard]] std::string displayName(const game::data::ActorData& actor) {
    return actor.display_name_.empty() ? actor.id_ : actor.display_name_;
}

} // namespace

namespace game::scene {

using namespace entt::literals;

RecruitOfferScene::RecruitOfferScene(std::string_view name,
                                     engine::core::Context& context,
                                     entt::registry& registry,
                                     const entt::entity player,
                                     const entt::entity recruiter,
                                     const game::data::ActorData& actor)
    : engine::scene::Scene(name, context),
      registry_(registry),
      player_(player),
      recruiter_(recruiter),
      actor_(actor),
      previous_state_(context.getGameState().getCurrentState()) {
}

RecruitOfferScene::~RecruitOfferScene() {
    disconnectRuntimeListeners();
    shutdownUI();
}

bool RecruitOfferScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Dialogue);
    context_pushed_ = true;

    refreshBindings();
    if (!initUI()) {
        return false;
    }

    connectRuntimeListeners();
    return Scene::init();
}

void RecruitOfferScene::clean() {
    shutdownUI();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

engine::scene::SceneUiCoverage RecruitOfferScene::uiCoverage() const {
    return engine::scene::SceneUiCoverage::HideUnderlyingSceneUi;
}

bool RecruitOfferScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("RecruitOfferScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME);
    if (!constructor) {
        spdlog::error("RecruitOfferScene: 创建 data model 失败。");
        return false;
    }

    constructor.Bind("speaker_text", &speaker_text_);
    constructor.Bind("offer_text", &offer_text_);
    constructor.Bind("actor_name", &actor_name_);
    constructor.Bind("portrait_player", &portrait_player_);
    constructor.Bind("portrait_lyria", &portrait_lyria_);
    constructor.Bind("portrait_tori", &portrait_tori_);

    if (!document_controller_.bindSimpleEvent(constructor, "accept", [this] { onAccept(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "decline", [this] { onDecline(); })) {
        spdlog::error("RecruitOfferScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("RecruitOfferScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    focusDefaultAction();
    return true;
}

void RecruitOfferScene::shutdownUI() {
    document_controller_.unload();
}

void RecruitOfferScene::connectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).connect<&RecruitOfferScene::onMenuCancelPressed>(this);
}

void RecruitOfferScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&RecruitOfferScene::onMenuCancelPressed>(this);
}

void RecruitOfferScene::refreshBindings() {
    const std::string name = displayName(actor_);
    speaker_text_ = makeRmlString(findSpeakerName(registry_, recruiter_));
    actor_name_ = makeRmlString(name);
    offer_text_ = makeRmlString("Invite " + name + " to join the party?");

    portrait_player_ = actor_.id_ == game::defs::kDefaultPlayerActorId;
    portrait_lyria_ = actor_.id_ == "actor.lyria";
    portrait_tori_ = actor_.id_ == "actor.tori";
}

void RecruitOfferScene::focusDefaultAction() {
    auto* document = document_controller_.document();
    if (!document) {
        return;
    }
    auto* accept_button = document->GetElementById("recruit-offer-accept-button");
    if (accept_button) {
        accept_button->Focus(true);
    }
}

bool RecruitOfferScene::onMenuCancelPressed() {
    onDecline();
    return true;
}

void RecruitOfferScene::onAccept() {
    context_.getDispatcher().trigger(game::defs::RecruitPartyMemberCommand{
        .player = player_,
        .recruiter = recruiter_,
        .actor_id_hash = actor_.id_hash_,
        .actor_id = actor_.id_});
    requestPopScene();
}

void RecruitOfferScene::onDecline() {
    requestPopScene();
}

} // namespace game::scene
