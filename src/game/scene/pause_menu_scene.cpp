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
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

using namespace entt::literals;

namespace {

constexpr float VOLUME_STEP = 0.10f;
constexpr float TIME_SCALE_MIN = 0.01f;
constexpr float TIME_SCALE_MAX = 100.0f;
constexpr float TIME_SCALE_STEP_RATIO_EXP = 0.2f;

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/pause_menu.rml";
constexpr std::string_view MODEL_NAME = "pause_menu";

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

std::string toPercentLabel(std::string_view prefix, float value) {
    const int percent = static_cast<int>(std::lround(clamp01(value) * 100.0f));
    return std::string(prefix) + " " + std::to_string(percent) + "%";
}

std::string toMultiplierLabel(std::string_view prefix, float value) {
    const float clamped = std::clamp(value, TIME_SCALE_MIN, TIME_SCALE_MAX);
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.2fx", clamped);
    return std::string(prefix) + " " + buffer;
}

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
    context_.getInputManager().onAction("pause"_hs).disconnect<&PauseMenuScene::onPausePressed>(this);
    context_.getDispatcher().sink<game::defs::AsyncSaveCompletedEvent>()
        .disconnect<&PauseMenuScene::onAsyncSaveCompleted>(this);
}

bool PauseMenuScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);

    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("pause"_hs).connect<&PauseMenuScene::onPausePressed>(this);
    context_.getDispatcher().sink<game::defs::AsyncSaveCompletedEvent>()
        .connect<&PauseMenuScene::onAsyncSaveCompleted>(this);

    if (!Scene::init()) {
        return false;
    }
    return true;
}

void PauseMenuScene::update(float delta_time) {
    refreshSaveActionButtons();

    if (close_after_load_) {
        close_after_load_ = false;
        requestPopScene();
    }

    Scene::update(delta_time);
}

void PauseMenuScene::clean() {
    removeEventListeners();
    data_bridge_.destroy();
    document_ = nullptr;

    context_.getGameState().setState(previous_state_);
    Scene::clean();
}

bool PauseMenuScene::initUI() {
    auto* rml_layer = context_.getGLRenderer().getRmlUILayer();
    if (!rml_layer || !rml_layer->getContext()) {
        spdlog::error("PauseMenuScene: RmlUILayer 或 Context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_layer->getContext(), MODEL_NAME);
    if (!constructor) {
        spdlog::error("PauseMenuScene: 创建 data model '{}' 失败。", MODEL_NAME);
        return false;
    }

    constructor.Bind("has_message", &has_message_);
    constructor.Bind("message_text", &message_text_);
    constructor.Bind("message_color", &message_color_);
    constructor.Bind("music_text", &music_text_);
    constructor.Bind("sound_text", &sound_text_);
    constructor.Bind("time_scale_text", &time_scale_text_);
    constructor.Bind("save_enabled", &save_enabled_);
    constructor.Bind("load_enabled", &load_enabled_);
    constructor.Bind("title_enabled", &title_enabled_);
    constructor.Bind("time_scale_enabled", &time_scale_enabled_);

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        spdlog::error("PauseMenuScene: 加载文档 '{}' 失败。", DOCUMENT_PATH);
        data_bridge_.destroy();
        return false;
    }

    bindEvents();
    refreshVolumeLabels();
    refreshTimeScaleLabel();
    refreshSaveActionButtons();
    setMessage("", false);
    data_bridge_.markAllDirty();
    return true;
}

void PauseMenuScene::bindEvents() {
    event_bridge_.on("resume", [this](Rml::Event&) { onResumeClicked(); });
    event_bridge_.on("save", [this](Rml::Event&) { onSaveClicked(); });
    event_bridge_.on("load", [this](Rml::Event&) { onLoadClicked(); });
    event_bridge_.on("back_to_title", [this](Rml::Event&) { onBackToTitleClicked(); });

    event_bridge_.on("time_down", [this](Rml::Event&) { adjustTimeScale(-1); });
    event_bridge_.on("time_up", [this](Rml::Event&) { adjustTimeScale(1); });
    event_bridge_.on("music_down", [this](Rml::Event&) { adjustMusicVolume(-1); });
    event_bridge_.on("music_up", [this](Rml::Event&) { adjustMusicVolume(1); });
    event_bridge_.on("sound_down", [this](Rml::Event&) { adjustSoundVolume(-1); });
    event_bridge_.on("sound_up", [this](Rml::Event&) { adjustSoundVolume(1); });

    if (document_) {
        event_bridge_.registerTo(document_, "click");
        click_listener_registered_ = true;
    }
}

void PauseMenuScene::removeEventListeners() {
    if (!click_listener_registered_ || !document_) {
        return;
    }

    document_->RemoveEventListener("click", &event_bridge_);
    click_listener_registered_ = false;
}

void PauseMenuScene::refreshVolumeLabels() {
    auto& audio = context_.getAudioPlayer();

    const Rml::String music_label = toPercentLabel("Music", audio.getMusicVolume());
    if (assignIfChanged(music_text_, music_label)) {
        data_bridge_.markDirty("music_text");
    }

    const Rml::String sound_label = toPercentLabel("SFX", audio.getSoundVolume());
    if (assignIfChanged(sound_text_, sound_label)) {
        data_bridge_.markDirty("sound_text");
    }
}

void PauseMenuScene::refreshTimeScaleLabel() {
    if (!game_time_) {
        if (assignIfChanged(time_scale_text_, Rml::String{"Speed N/A"})) {
            data_bridge_.markDirty("time_scale_text");
        }
        if (assignIfChanged(time_scale_enabled_, false)) {
            data_bridge_.markDirty("time_scale_enabled");
        }
        return;
    }

    float scale = game_time_->time_scale_;
    if (!std::isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }
    scale = std::clamp(scale, TIME_SCALE_MIN, TIME_SCALE_MAX);
    game_time_->time_scale_ = scale;

    const Rml::String next_label = toMultiplierLabel("Speed", scale);
    if (assignIfChanged(time_scale_text_, next_label)) {
        data_bridge_.markDirty("time_scale_text");
    }
    if (assignIfChanged(time_scale_enabled_, true)) {
        data_bridge_.markDirty("time_scale_enabled");
    }
}

void PauseMenuScene::refreshSaveActionButtons() {
    const bool has_save_service = (save_service_ != nullptr);
    const bool saving = has_save_service && save_service_->isSaving();

    const bool next_save_enabled = has_save_service && !saving;
    const bool next_load_enabled = has_save_service && !saving;
    const bool next_title_enabled = !saving;

    if (assignIfChanged(save_enabled_, next_save_enabled)) {
        data_bridge_.markDirty("save_enabled");
    }
    if (assignIfChanged(load_enabled_, next_load_enabled)) {
        data_bridge_.markDirty("load_enabled");
    }
    if (assignIfChanged(title_enabled_, next_title_enabled)) {
        data_bridge_.markDirty("title_enabled");
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
    const bool next_has_message = !message.empty();
    if (assignIfChanged(has_message_, next_has_message)) {
        data_bridge_.markDirty("has_message");
    }

    const Rml::String next_text = message;
    if (assignIfChanged(message_text_, next_text)) {
        data_bridge_.markDirty("message_text");
    }

    const Rml::String next_color = is_error ? Rml::String{"#ff6e6e"} : Rml::String{"#6ee7a1"};
    if (assignIfChanged(message_color_, next_color)) {
        data_bridge_.markDirty("message_color");
    }
}

bool PauseMenuScene::onPausePressed() {
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
        requestPopScene(); // close SaveSlotSelectScene
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
            requestPopScene(); // close SaveSlotSelectScene
            return;
        }

        setMessage("Loaded", false);
        requestPopScene();   // close SaveSlotSelectScene
        close_after_load_ = true; // close menu on next frame
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
        const auto exp_before = std::log10(scale);
        const auto exp_after = exp_before + TIME_SCALE_STEP_RATIO_EXP;
        scale = std::pow(10.0f, exp_after);
    } else if (step < 0) {
        const auto exp_before = std::log10(scale);
        const auto exp_after = exp_before - TIME_SCALE_STEP_RATIO_EXP;
        scale = std::pow(10.0f, exp_after);
    }
    scale = std::clamp(scale, TIME_SCALE_MIN, TIME_SCALE_MAX);

    game_time_->time_scale_ = scale;
    refreshTimeScaleLabel();
}

} // namespace game::scene
