#include "dialogue_choice_scene.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/dialogue_choice.rml";
constexpr std::string_view MODEL_NAME = "dialogue_choice";

[[nodiscard]] Rml::String makeRmlString(const std::string_view value) {
    return Rml::String{value.data(), value.size()};
}

} // namespace

namespace game::scene {

using namespace entt::literals;

DialogueChoiceScene::DialogueChoiceScene(std::string_view name,
                                         engine::core::Context& context,
                                         game::defs::DialogueChoiceRequestedEvent request)
    : engine::scene::Scene(name, context),
      request_(std::move(request)),
      previous_state_(context.getGameState().getCurrentState()) {
}

DialogueChoiceScene::~DialogueChoiceScene() {
    disconnectRuntimeListeners();
    shutdownUI();
}

bool DialogueChoiceScene::init() {
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

void DialogueChoiceScene::clean() {
    shutdownUI();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

engine::scene::SceneUiCoverage DialogueChoiceScene::uiCoverage() const {
    return engine::scene::SceneUiCoverage::HideUnderlyingSceneUi;
}

bool DialogueChoiceScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("DialogueChoiceScene: RmlUiRuntime 不可用。");
        return false;
    }

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("DialogueChoiceScene: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("DialogueChoiceScene: 注册 choice data types 失败。");
        document_controller_.unload();
        return false;
    }

    if (!constructor.Bind("speaker_text", &speaker_text_) ||
        !constructor.Bind("prompt_text", &prompt_text_) ||
        !constructor.Bind("has_speaker", &has_speaker_) ||
        !constructor.Bind("allow_cancel", &allow_cancel_) ||
        !constructor.Bind("choices", &choices_)) {
        spdlog::error("DialogueChoiceScene: 绑定 data model 变量失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.bindEvent(
            constructor,
            "choose",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) {
                onChoose(resolveClickedChoiceIndex(event));
            }) ||
        !document_controller_.bindSimpleEvent(constructor, "cancel", [this] { onCancel(); })) {
        spdlog::error("DialogueChoiceScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("DialogueChoiceScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    document_controller_.markAllDirty();
    focusDefaultAction();
    return true;
}

bool DialogueChoiceScene::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (auto choice_handle = constructor.RegisterStruct<ChoiceViewModel>()) {
        choice_handle.RegisterMember("option_index", &ChoiceViewModel::option_index);
        choice_handle.RegisterMember("label", &ChoiceViewModel::label);
    } else {
        return false;
    }

    if (!constructor.RegisterArray<decltype(choices_)>()) {
        return false;
    }

    data_types_registered_ = true;
    return true;
}

void DialogueChoiceScene::shutdownUI() {
    document_controller_.unload();
}

void DialogueChoiceScene::connectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).connect<&DialogueChoiceScene::onMenuCancelPressed>(this);
}

void DialogueChoiceScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&DialogueChoiceScene::onMenuCancelPressed>(this);
}

void DialogueChoiceScene::refreshBindings() {
    speaker_text_ = makeRmlString(request_.speaker);
    prompt_text_ = makeRmlString(request_.prompt);
    has_speaker_ = !request_.speaker.empty();
    allow_cancel_ = request_.allow_cancel;

    choices_.clear();
    choices_.reserve(request_.options.size());
    for (std::size_t index = 0; index < request_.options.size(); ++index) {
        choices_.push_back(ChoiceViewModel{
            .option_index = static_cast<int>(index),
            .label = makeRmlString(request_.options[index].label),
        });
    }
}

void DialogueChoiceScene::focusDefaultAction() {
    auto* document = document_controller_.document();
    if (!document) {
        return;
    }

    auto* option_list = document->GetElementById("dialogue-choice-options");
    if (option_list && option_list->GetNumChildren() > 0) {
        option_list->GetChild(0)->Focus(true);
        return;
    }

    auto* cancel_button = document->GetElementById("dialogue-choice-cancel-button");
    if (cancel_button) {
        cancel_button->Focus(true);
    }
}

int DialogueChoiceScene::resolveClickedChoiceIndex(const Rml::Event& event) const {
    const Rml::Element* element = event.GetCurrentElement();
    if (!element) {
        element = event.GetTargetElement();
    }
    return element ? element->GetAttribute<int>("data-choice-index", -1) : -1;
}

bool DialogueChoiceScene::onMenuCancelPressed() {
    onCancel();
    return true;
}

void DialogueChoiceScene::onChoose(const int option_index) {
    if (option_index < 0 || option_index >= static_cast<int>(request_.options.size())) {
        return;
    }

    const auto& option = request_.options[static_cast<std::size_t>(option_index)];
    context_.getDispatcher().trigger(game::defs::DialogueChoiceSelectedEvent{
        .request_id = request_.request_id,
        .target = request_.target,
        .option_index = option_index,
        .choice_id = option.id,
        .choice_label = option.label,
        .cancelled = false,
    });
    requestPopScene();
}

void DialogueChoiceScene::onCancel() {
    if (!request_.allow_cancel) {
        return;
    }

    context_.getDispatcher().trigger(game::defs::DialogueChoiceSelectedEvent{
        .request_id = request_.request_id,
        .target = request_.target,
        .option_index = -1,
        .choice_id = {},
        .choice_label = {},
        .cancelled = true,
    });
    requestPopScene();
}

} // namespace game::scene
