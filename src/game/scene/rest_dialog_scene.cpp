#include "rest_dialog_scene.h"

#include "game/defs/events.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"

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
    shutdownUI();
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
    shutdownUI();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool RestDialogScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("RestDialogScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME);
    if (!constructor) {
        spdlog::error("RestDialogScene: 创建 data model 失败。");
        return false;
    }

    constructor.Bind("hours_text", &hours_text_);

    if (!document_controller_.bindSimpleEvent(constructor, "hours_down", [this] { adjustHours(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "hours_up", [this] { adjustHours(1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "confirm", [this] { onConfirm(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "cancel", [this] { onCancel(); })) {
        spdlog::error("RestDialogScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    document_controller_.setDefaultFocusById("rest-hours-down-button");
    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("RestDialogScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    updateHoursLabel();
    return true;
}

void RestDialogScene::shutdownUI() {
    document_controller_.unload();
}

void RestDialogScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&RestDialogScene::onMenuCancelPressed>(this);
}

void RestDialogScene::updateHoursLabel() {
    const auto text = std::to_string(selected_hours_) + "h";
    if (updateBoundString(hours_text_, text)) {
        document_controller_.markDirty("hours_text");
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
