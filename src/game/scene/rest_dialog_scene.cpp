#include "rest_dialog_scene.h"

#include "game/defs/events.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/rest_dialog.rml";
constexpr std::string_view MODEL_NAME = "rest_dialog";

template <typename T>
bool assignIfChanged(T& target, const T& value) {
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

} // namespace

namespace game::scene {

RestDialogScene::RestDialogScene(std::string_view name, engine::core::Context& context)
    : engine::scene::Scene(name, context),
      previous_state_(context.getGameState().getCurrentState()) {
}

bool RestDialogScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);

    if (!initUI()) {
        return false;
    }

    if (!Scene::init()) {
        return false;
    }

    updateHoursLabel();
    return true;
}

void RestDialogScene::clean() {
    removeEventListeners();
    data_bridge_.destroy();
    document_ = nullptr;

    context_.getGameState().setState(previous_state_);
    Scene::clean();
}

bool RestDialogScene::initUI() {
    auto* rml_layer = context_.getGLRenderer().getRmlUILayer();
    if (!rml_layer || !rml_layer->getContext()) {
        return false;
    }

    auto constructor = data_bridge_.create(rml_layer->getContext(), MODEL_NAME);
    if (!constructor) {
        return false;
    }

    constructor.Bind("hours_text", &hours_text_);

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        data_bridge_.destroy();
        return false;
    }

    bindEvents();
    data_bridge_.markAllDirty();
    return true;
}

void RestDialogScene::bindEvents() {
    event_bridge_.on("hours_down", [this](Rml::Event&) { adjustHours(-1); });
    event_bridge_.on("hours_up", [this](Rml::Event&) { adjustHours(1); });
    event_bridge_.on("confirm", [this](Rml::Event&) { onConfirm(); });
    event_bridge_.on("cancel", [this](Rml::Event&) { onCancel(); });

    if (document_) {
        event_bridge_.registerTo(document_, "click");
        click_listener_registered_ = true;
    }
}

void RestDialogScene::removeEventListeners() {
    if (!click_listener_registered_ || !document_) {
        return;
    }

    document_->RemoveEventListener("click", &event_bridge_);
    click_listener_registered_ = false;
}

void RestDialogScene::updateHoursLabel() {
    const Rml::String next_text = std::to_string(selected_hours_) + "h";
    if (assignIfChanged(hours_text_, next_text)) {
        data_bridge_.markDirty("hours_text");
    }
}

void RestDialogScene::adjustHours(int delta) {
    const int next = std::clamp(selected_hours_ + delta, 1, 24);
    if (next == selected_hours_) {
        return;
    }

    selected_hours_ = next;
    updateHoursLabel();
}

void RestDialogScene::onConfirm() {
    context_.getDispatcher().enqueue(game::defs::AdvanceTimeRequest{selected_hours_});
    // TODO: Dispatch a recovery event (health/stamina) once related systems are implemented.
    requestPopScene();
}

void RestDialogScene::onCancel() {
    requestPopScene();
}

} // namespace game::scene
