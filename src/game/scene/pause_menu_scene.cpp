#include "pause_menu_scene.h"

#include "save_slot_select_scene.h"
#include "title_scene.h"

#include "game/data/game_time.h"
#include "game/defs/events.h"
#include "game/save/save_service.h"

#include "engine/audio/audio_player.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/hover_focus_sync_listener.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

using namespace entt::literals;

namespace {

constexpr float VOLUME_STEP = 0.10f;
constexpr float TIME_SCALE_MIN = 0.01f;
constexpr float TIME_SCALE_MAX = 100.0f;
constexpr float TIME_SCALE_STEP_RATIO_EXP = 0.2f;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/pause_menu.rml";
constexpr std::string_view MODEL_NAME = "pause_menu";

[[nodiscard]] float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] std::string toPercentLabel(std::string_view prefix, float value) {
    const int percent = static_cast<int>(std::lround(clamp01(value) * 100.0f));
    return std::string(prefix) + " " + std::to_string(percent) + "%";
}

[[nodiscard]] std::string toMultiplierLabel(std::string_view prefix, float value) {
    const float clamped = std::clamp(value, TIME_SCALE_MIN, TIME_SCALE_MAX);
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.2fx", clamped);
    return std::string(prefix) + " " + buffer;
}

using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;

} // namespace

namespace game::scene {

PauseMenuScene::PauseMenuScene(std::string_view name,
                               engine::core::Context& context,
                               game::save::SaveService* save_service,
                               game::data::GameTime* game_time)
    : engine::scene::Scene(name, context),
      save_service_(save_service),
      game_time_(game_time),
      previous_state_(context.getGameState().getCurrentState()) {
}

PauseMenuScene::~PauseMenuScene() {
    disconnectRuntimeListeners();
    shutdownUI();
}

bool PauseMenuScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("menu_cancel"_hs).connect<&PauseMenuScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::AsyncSaveCompletedEvent>()
        .connect<&PauseMenuScene::onAsyncSaveCompleted>(this);

    if (!Scene::init()) {
        return false;
    }
    return true;
}

void PauseMenuScene::update(float delta_time) {
    refreshVolumeLabels();
    refreshTimeScaleLabel();
    refreshSaveActionButtons();

    if (close_after_load_) {
        close_after_load_ = false;
        requestPopScene();
    }

    Scene::update(delta_time);
}

void PauseMenuScene::clean() {
    shutdownUI();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool PauseMenuScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("PauseMenuScene: RmlUiRuntime 不可用。");
        return false;
    }

    auto* rml_context = runtime->getContext();
    if (!rml_context) {
        spdlog::error("PauseMenuScene: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME);
    if (!constructor) {
        spdlog::error("PauseMenuScene: 创建 data model 失败。");
        return false;
    }

    constructor.Bind("message_text", &message_text_);
    constructor.Bind("has_message", &has_message_);
    constructor.Bind("message_is_error", &message_is_error_);
    constructor.Bind("music_text", &music_text_);
    constructor.Bind("sound_text", &sound_text_);
    constructor.Bind("speed_text", &speed_text_);
    constructor.Bind("can_save", &can_save_);
    constructor.Bind("can_load", &can_load_);
    constructor.Bind("can_back_title", &can_back_title_);

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        data_bridge_.destroy();
        spdlog::error("PauseMenuScene: 加载 RML 文档失败。");
        return false;
    }

    event_bridge_.on("resume", [this](Rml::Event&) { onResumeClicked(); });
    event_bridge_.on("save", [this](Rml::Event&) { onSaveClicked(); });
    event_bridge_.on("load", [this](Rml::Event&) { onLoadClicked(); });
    event_bridge_.on("back_to_title", [this](Rml::Event&) { onBackToTitleClicked(); });
    event_bridge_.on("speed_down", [this](Rml::Event&) { adjustTimeScale(-1); });
    event_bridge_.on("speed_up", [this](Rml::Event&) { adjustTimeScale(1); });
    event_bridge_.on("music_down", [this](Rml::Event&) { adjustMusicVolume(-1); });
    event_bridge_.on("music_up", [this](Rml::Event&) { adjustMusicVolume(1); });
    event_bridge_.on("sound_down", [this](Rml::Event&) { adjustSoundVolume(-1); });
    event_bridge_.on("sound_up", [this](Rml::Event&) { adjustSoundVolume(1); });
    event_bridge_.registerTo(document_, "click");
    hover_focus_listener_ = std::make_unique<engine::ui::rmlui::HoverFocusSyncListener>(*runtime);
    hover_focus_listener_->registerTo(document_, "mouseover");

    refreshVolumeLabels();
    refreshTimeScaleLabel();
    refreshSaveActionButtons();
    setMessage("", true);
    data_bridge_.markAllDirty();
    runtime->queueFocusElementById(document_, "pause-resume-button");
    return true;
}

void PauseMenuScene::shutdownUI() {
    event_bridge_.unregisterAll();
    if (hover_focus_listener_) {
        hover_focus_listener_->unregisterAll();
    }
    unloadAllRmlDocuments();
    document_ = nullptr;
    data_bridge_.destroy();
}

void PauseMenuScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&PauseMenuScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::AsyncSaveCompletedEvent>()
        .disconnect<&PauseMenuScene::onAsyncSaveCompleted>(this);
}

void PauseMenuScene::refreshVolumeLabels() {
    auto& audio = context_.getAudioPlayer();

    if (updateBoundString(music_text_, toPercentLabel("Music", audio.getMusicVolume()))) {
        data_bridge_.markDirty("music_text");
    }
    if (updateBoundString(sound_text_, toPercentLabel("SFX", audio.getSoundVolume()))) {
        data_bridge_.markDirty("sound_text");
    }
}

void PauseMenuScene::refreshTimeScaleLabel() {
    if (!game_time_) {
        return;
    }

    float scale = game_time_->time_scale_;
    if (!std::isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }

    if (updateBoundString(speed_text_, toMultiplierLabel("Speed", scale))) {
        data_bridge_.markDirty("speed_text");
    }
}

void PauseMenuScene::refreshSaveActionButtons() {
    const bool has_save_service = (save_service_ != nullptr);
    const bool saving = has_save_service && save_service_->isSaving();

    if (updateBoundBool(can_save_, has_save_service && !saving)) {
        data_bridge_.markDirty("can_save");
    }
    if (updateBoundBool(can_load_, has_save_service && !saving)) {
        data_bridge_.markDirty("can_load");
    }
    if (updateBoundBool(can_back_title_, !saving)) {
        data_bridge_.markDirty("can_back_title");
    }
}

void PauseMenuScene::onAsyncSaveCompleted(const game::defs::AsyncSaveCompletedEvent& event) {
    if (event.success) {
        setMessage("Saved", false);
        refreshSaveActionButtons();
        return;
    }

    std::string message = "Save failed";
    if (!event.error.empty()) {
        message += ": " + event.error;
    }
    setMessage(std::move(message), true);
    refreshSaveActionButtons();
}

void PauseMenuScene::setMessage(std::string message, bool is_error) {
    if (updateBoundString(message_text_, message)) {
        data_bridge_.markDirty("message_text");
    }
    if (updateBoundBool(has_message_, !message.empty())) {
        data_bridge_.markDirty("has_message");
    }
    if (updateBoundBool(message_is_error_, is_error)) {
        data_bridge_.markDirty("message_is_error");
    }
}

bool PauseMenuScene::onMenuCancelPressed() {
    requestPopScene();
    return true;
}

void PauseMenuScene::onResumeClicked() {
    requestPopScene();
}

void PauseMenuScene::onSaveClicked() {
    setMessage("", false);

    if (!save_service_) {
        setMessage("SaveService unavailable", true);
        return;
    }

    auto on_select = [this](int slot) {
        std::string error;
        if (!save_service_->saveToFileAsync(game::save::SaveService::slotPath(slot), error)) {
            setMessage("Save failed: " + error, true);
        } else {
            setMessage("Saving...", false);
        }
        refreshSaveActionButtons();
        requestPopScene();
    };

    auto select = std::make_unique<game::scene::SaveSlotSelectScene>(
        "SaveSlotSelect", context_, std::move(on_select), game::scene::SaveSlotSelectScene::Mode::Save);
    requestPushScene(std::move(select));
}

void PauseMenuScene::onLoadClicked() {
    setMessage("", false);

    if (!save_service_) {
        setMessage("SaveService unavailable", true);
        return;
    }
    if (save_service_->isSaving()) {
        setMessage("Save in progress", true);
        return;
    }

    auto on_select = [this](int slot) {
        std::string error;
        if (!save_service_->loadFromFile(game::save::SaveService::slotPath(slot), error)) {
            setMessage("Load failed: " + error, true);
            requestPopScene();
            return;
        }

        setMessage("Loaded", false);
        requestPopScene();
        close_after_load_ = true;
    };

    auto select = std::make_unique<game::scene::SaveSlotSelectScene>(
        "SaveSlotSelect", context_, std::move(on_select), game::scene::SaveSlotSelectScene::Mode::Load);
    requestPushScene(std::move(select));
}

void PauseMenuScene::onBackToTitleClicked() {
    if (save_service_ && save_service_->isSaving()) {
        setMessage("Save in progress", true);
        return;
    }
    requestReplaceScene(std::make_unique<game::scene::TitleScene>("TitleScene", context_));
}

void PauseMenuScene::adjustMusicVolume(int step) {
    auto& audio = context_.getAudioPlayer();
    const float next = clamp01(audio.getMusicVolume() + static_cast<float>(step) * VOLUME_STEP);
    audio.setMusicVolume(next);
    refreshVolumeLabels();
}

void PauseMenuScene::adjustSoundVolume(int step) {
    auto& audio = context_.getAudioPlayer();
    const float next = clamp01(audio.getSoundVolume() + static_cast<float>(step) * VOLUME_STEP);
    audio.setSoundVolume(next);
    refreshVolumeLabels();
}

void PauseMenuScene::adjustTimeScale(int step) {
    if (!game_time_) {
        return;
    }

    float scale = game_time_->time_scale_;
    if (!std::isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }
    scale = std::clamp(scale, TIME_SCALE_MIN, TIME_SCALE_MAX);

    if (step > 0) {
        scale = std::pow(10.0f, std::log10(scale) + TIME_SCALE_STEP_RATIO_EXP);
    } else if (step < 0) {
        scale = std::pow(10.0f, std::log10(scale) - TIME_SCALE_STEP_RATIO_EXP);
    }
    scale = std::clamp(scale, TIME_SCALE_MIN, TIME_SCALE_MAX);

    game_time_->time_scale_ = scale;
    refreshTimeScaleLabel();
}

} // namespace game::scene
