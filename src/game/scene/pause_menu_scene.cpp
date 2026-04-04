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
#include "engine/ui/rmlui/rml_bind_helpers.h"

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <format>
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
    return std::format("{} {:.2f}x", prefix, clamped);
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

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME);
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

    if (!document_controller_.bindSimpleEvent(constructor, "resume", [this] { onResumeClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "save", [this] { onSaveClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "load", [this] { onLoadClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "back_to_title", [this] { onBackToTitleClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "speed_down", [this] { adjustTimeScale(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "speed_up", [this] { adjustTimeScale(1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "music_down", [this] { adjustMusicVolume(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "music_up", [this] { adjustMusicVolume(1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "sound_down", [this] { adjustSoundVolume(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "sound_up", [this] { adjustSoundVolume(1); })) {
        spdlog::error("PauseMenuScene: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("PauseMenuScene: 加载 RML 文档失败。");
        document_controller_.unload();
        return false;
    }

    refreshVolumeLabels();
    refreshTimeScaleLabel();
    refreshSaveActionButtons();
    setMessage("", false);
    document_controller_.markAllDirty();
    return true;
}

void PauseMenuScene::shutdownUI() {
    document_controller_.unload();
}

void PauseMenuScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs).disconnect<&PauseMenuScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::AsyncSaveCompletedEvent>()
        .disconnect<&PauseMenuScene::onAsyncSaveCompleted>(this);
}

void PauseMenuScene::refreshVolumeLabels() {
    auto& audio = context_.getAudioPlayer();

    if (updateBoundString(music_text_, toPercentLabel("Music", audio.getMusicVolume()))) {
        document_controller_.markDirty("music_text");
    }
    if (updateBoundString(sound_text_, toPercentLabel("SFX", audio.getSoundVolume()))) {
        document_controller_.markDirty("sound_text");
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
        document_controller_.markDirty("speed_text");
    }
}

void PauseMenuScene::refreshSaveActionButtons() {
    const bool has_save_service = (save_service_ != nullptr);
    const bool saving = has_save_service && save_service_->isSaving();

    if (updateBoundBool(can_save_, has_save_service && !saving)) {
        document_controller_.markDirty("can_save");
    }
    if (updateBoundBool(can_load_, has_save_service && !saving)) {
        document_controller_.markDirty("can_load");
    }
    if (updateBoundBool(can_back_title_, !saving)) {
        document_controller_.markDirty("can_back_title");
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
        document_controller_.markDirty("message_text");
    }
    if (updateBoundBool(has_message_, !message.empty())) {
        document_controller_.markDirty("has_message");
    }
    if (updateBoundBool(message_is_error_, is_error)) {
        document_controller_.markDirty("message_is_error");
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
