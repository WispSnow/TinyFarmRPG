#include "rest_dialog_scene.h"

#include "game/defs/events.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/rest_dialog.rml";
constexpr std::string_view MODEL_NAME = "rest_dialog";

using engine::ui::rmlui::updateBoundString;

} // namespace

namespace game::scene {

using namespace entt::literals;

RestDialogScene::RestDialogScene(std::string_view name, engine::core::Context& context)
    : engine::scene::Scene(name, context),
      previous_state_(context.getGameState().getCurrentState()) {
}

RestDialogScene::~RestDialogScene() {
    disconnectRuntimeListeners();
    if (document_ || data_bridge_.isValid() || click_listener_registered_) {
        removeEventListeners();
        if (document_) {
            unloadAllRmlDocuments();
            document_ = nullptr;
        }
        data_bridge_.destroy();
    }
}

bool RestDialogScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Dialogue);
    context_pushed_ = true;

    if (!initUI()) {
        return false;
    }
    context_.getInputManager().onAction("menu_cancel"_hs).connect<&RestDialogScene::onMenuCancelPressed>(this);
    if (!Scene::init()) {
        return false;
    }

    return true;
}

void RestDialogScene::clean() {
    disconnectRuntimeListeners();
    removeEventListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
    document_ = nullptr;
    data_bridge_.destroy();
}

bool RestDialogScene::initUI() {
    auto* layer = context_.getGLRenderer().getRmlUILayer();
    if (!layer) {
        spdlog::error("RestDialogScene: RmlUILayer 不可用。");
        return false;
    }

    auto* rml_context = layer->getContext();
    if (!rml_context) {
        spdlog::error("RestDialogScene: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME);
    if (!constructor) {
        spdlog::error("RestDialogScene: 创建 data model 失败。");
        return false;
    }

    constructor.Bind("hours_text", &hours_text_);

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        data_bridge_.destroy();
        spdlog::error("RestDialogScene: 加载 RML 文档失败。");
        return false;
    }

    event_bridge_.on("hours_down", [this](Rml::Event&) { adjustHours(-1); });
    event_bridge_.on("hours_up", [this](Rml::Event&) { adjustHours(1); });
    event_bridge_.on("confirm", [this](Rml::Event&) { onConfirm(); });
    event_bridge_.on("cancel", [this](Rml::Event&) { onCancel(); });
    event_bridge_.registerTo(document_, "click");
    click_listener_registered_ = true;

    updateHoursLabel();
    layer->queueFocusElementById(document_, "rest-hours-down-button");
    return true;
}

void RestDialogScene::removeEventListeners() {
    if (document_ && click_listener_registered_) {
        document_->RemoveEventListener("click", &event_bridge_);
        click_listener_registered_ = false;
    }
}

void RestDialogScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&RestDialogScene::onMenuCancelPressed>(this);
}

void RestDialogScene::updateHoursLabel() {
    const auto text = std::to_string(selected_hours_) + "h";
    if (updateBoundString(hours_text_, text)) {
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

bool RestDialogScene::onMenuCancelPressed() {
    onCancel();
    return true;
}

void RestDialogScene::onConfirm() {
    context_.getDispatcher().enqueue(game::defs::AdvanceTimeRequest{selected_hours_});
    requestPopScene();
}

void RestDialogScene::onCancel() {
    requestPopScene();
}

} // namespace game::scene
