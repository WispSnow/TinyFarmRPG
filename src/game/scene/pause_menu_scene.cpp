#include "pause_menu_scene.h"

#include "save_slot_select_scene.h"
#include "title_scene.h"

#include "game/data/game_time.h"
#include "game/defs/events.h"
#include "game/defs/options_events.h"
#include "game/runtime/localization_service.h"
#include "game/runtime/user_settings_service.h"
#include "game/save/save_service.h"
#include "game/ui/localized_text.h"

#include "engine/audio/audio_player.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/script/script_host.h"
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

[[nodiscard]] std::string toPercentLabel(const game::runtime::LocalizationService* localization,
                                         std::string_view key,
                                         std::string_view fallback_prefix,
                                         float value) {
    const int percent = static_cast<int>(std::lround(clamp01(value) * 100.0f));
    const auto percent_text = std::to_string(percent);
    return game::ui::formatTextOrFallback(localization,
                                          key,
                                          game::ui::LocalizedFormatArgs{{"percent", percent_text}},
                                          [&] { return std::string(fallback_prefix) + " " + percent_text + "%"; });
}

[[nodiscard]] std::string toMultiplierLabel(const game::runtime::LocalizationService* localization,
                                            std::string_view key,
                                            std::string_view fallback_prefix,
                                            float value) {
    const float clamped = std::clamp(value, TIME_SCALE_MIN, TIME_SCALE_MAX);
    const auto value_text = std::format("{:.2f}", clamped);
    return game::ui::formatTextOrFallback(localization,
                                          key,
                                          game::ui::LocalizedFormatArgs{{"value", value_text}},
                                          [&] { return std::format("{} {}x", fallback_prefix, value_text); });
}

[[nodiscard]] std::size_t indexOfLanguage(const game::runtime::LocalizationService& localization,
                                          const std::string_view language_tag) noexcept {
    const auto& languages = localization.languages();
    for (std::size_t i = 0; i < languages.size(); ++i) {
        if (languages[i].tag == language_tag) {
            return i;
        }
    }
    return 0;
}

using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;

} // namespace

namespace game::scene {

PauseMenuScene::PauseMenuScene(std::string_view name,
                               engine::core::Context& context,
                               game::save::SaveService* save_service,
                               game::data::GameTime* game_time,
                               game::runtime::UserSettingsService* user_settings_service,
                               engine::script::ScriptHost* script_host)
    : engine::scene::Scene(name, context),
      save_service_(save_service),
      game_time_(game_time),
      user_settings_service_(user_settings_service),
      script_host_(script_host),
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
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>().connect<&PauseMenuScene::onLanguageChanged>(this);

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
    if (user_settings_service_) {
        (void)user_settings_service_->flushIfDirty();
    }
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
    constructor.Bind("language_text", &language_text_);
    constructor.Bind("can_save", &can_save_);
    constructor.Bind("can_load", &can_load_);
    constructor.Bind("can_back_title", &can_back_title_);
    constructor.Bind("can_change_language", &can_change_language_);

    if (!document_controller_.bindSimpleEvent(constructor, "resume", [this] { onResumeClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "save", [this] { onSaveClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "load", [this] { onLoadClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "back_to_title", [this] { onBackToTitleClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "speed_down", [this] { adjustTimeScale(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "speed_up", [this] { adjustTimeScale(1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "language_down", [this] { adjustLanguage(-1); }) ||
        !document_controller_.bindSimpleEvent(constructor, "language_up", [this] { adjustLanguage(1); }) ||
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
    refreshLanguageLabel();
    refreshSaveActionButtons();
    clearMessage();
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
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>()
        .disconnect<&PauseMenuScene::onLanguageChanged>(this);
}

void PauseMenuScene::refreshVolumeLabels() {
    auto& audio = context_.getAudioPlayer();

    if (updateBoundString(music_text_,
                          toPercentLabel(localization(), "pause.value.music", "Music", audio.getMusicVolume()))) {
        document_controller_.markDirty("music_text");
    }
    if (updateBoundString(sound_text_,
                          toPercentLabel(localization(), "pause.value.sound", "SFX", audio.getSoundVolume()))) {
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

    if (updateBoundString(speed_text_, toMultiplierLabel(localization(), "pause.value.speed", "Speed", scale))) {
        document_controller_.markDirty("speed_text");
    }
}

void PauseMenuScene::refreshLanguageLabel() {
    std::string label{"English"};
    bool can_change = false;

    if (user_settings_service_) {
        const auto& localization_service = user_settings_service_->localization();
        const auto& languages = localization_service.languages();
        const auto& settings = user_settings_service_->snapshot();
        label = localization_service.languageNativeName(settings.language_tag);
        can_change = languages.size() > 1;
    }

    if (updateBoundString(language_text_, label)) {
        document_controller_.markDirty("language_text");
    }
    if (updateBoundBool(can_change_language_, can_change)) {
        document_controller_.markDirty("can_change_language");
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

void PauseMenuScene::refreshLocalizedBindings() {
    refreshVolumeLabels();
    refreshTimeScaleLabel();
    refreshLanguageLabel();
    if (!message_key_.empty()) {
        publishMessage(resolveMessageText(), message_is_error_);
    }
}

void PauseMenuScene::onAsyncSaveCompleted(const game::defs::AsyncSaveCompletedEvent& event) {
    if (event.success) {
        setLocalizedMessage("pause.message.saved", false, {}, "Saved");
        refreshSaveActionButtons();
        return;
    }

    if (!event.error.empty()) {
        setLocalizedMessage("pause.message.save_failed_detail",
                            true,
                            {{"error", event.error}},
                            "Save failed: " + event.error);
        refreshSaveActionButtons();
        return;
    }
    setLocalizedMessage("pause.message.save_failed", true, {}, "Save failed");
    refreshSaveActionButtons();
}

void PauseMenuScene::onLanguageChanged(const game::defs::LanguageChangedEvent&) {
    refreshLocalizedBindings();
}

const game::runtime::LocalizationService* PauseMenuScene::localization() const noexcept {
    return user_settings_service_ ? &user_settings_service_->localization() : nullptr;
}

std::string PauseMenuScene::resolveMessageText() const {
    if (message_key_.empty()) {
        return {};
    }
    return game::ui::formatTextOrFallback(localization(),
                                          message_key_,
                                          message_args_,
                                          [this] { return message_fallback_; });
}

void PauseMenuScene::clearMessage() {
    message_key_.clear();
    message_args_.clear();
    message_fallback_.clear();
    publishMessage("", false);
}

void PauseMenuScene::setLocalizedMessage(std::string_view key,
                                         bool is_error,
                                         std::unordered_map<std::string, std::string> args,
                                         std::string fallback) {
    message_key_ = std::string{key};
    message_args_ = std::move(args);
    message_fallback_ = std::move(fallback);
    publishMessage(resolveMessageText(), is_error);
}

void PauseMenuScene::publishMessage(std::string message, bool is_error) {
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
    clearMessage();

    if (!save_service_) {
        setLocalizedMessage("pause.message.save_service_unavailable", true, {}, "SaveService unavailable");
        return;
    }

    auto on_select = [this](int slot) {
        std::string error;
        if (!save_service_->saveToFileAsync(game::save::SaveService::slotPath(slot), error)) {
            setLocalizedMessage("pause.message.save_failed_detail",
                                true,
                                {{"error", error}},
                                "Save failed: " + error);
        } else {
            setLocalizedMessage("pause.message.saving", false, {}, "Saving...");
        }
        refreshSaveActionButtons();
        requestPopScene();
    };

    auto select = std::make_unique<game::scene::SaveSlotSelectScene>(
        "SaveSlotSelect",
        context_,
        std::move(on_select),
        game::scene::SaveSlotSelectScene::Mode::Save,
        localization());
    requestPushScene(std::move(select));
}

void PauseMenuScene::onLoadClicked() {
    clearMessage();

    if (!save_service_) {
        setLocalizedMessage("pause.message.save_service_unavailable", true, {}, "SaveService unavailable");
        return;
    }
    if (save_service_->isSaving()) {
        setLocalizedMessage("pause.message.save_in_progress", true, {}, "Save in progress");
        return;
    }

    auto on_select = [this](int slot) {
        std::string error;
        if (!save_service_->loadFromFile(game::save::SaveService::slotPath(slot), error)) {
            setLocalizedMessage("pause.message.load_failed_detail",
                                true,
                                {{"error", error}},
                                "Load failed: " + error);
            requestPopScene();
            return;
        }

        // SaveService::apply 会把 ctx GameTime::time_scale_ 覆写为存档值；
        // 用户偏好是 global 概念，不应被存档覆盖，所以这里立即把当前偏好重新 apply 一次。
        if (user_settings_service_) {
            user_settings_service_->applyAll();
        }
        if (script_host_ && !script_host_->reload()) {
            spdlog::warn("PauseMenuScene: 读档后重新加载脚本失败。");
        }

        setLocalizedMessage("pause.message.loaded", false, {}, "Loaded");
        requestPopScene();
        close_after_load_ = true;
    };

    auto select = std::make_unique<game::scene::SaveSlotSelectScene>(
        "SaveSlotSelect",
        context_,
        std::move(on_select),
        game::scene::SaveSlotSelectScene::Mode::Load,
        localization());
    requestPushScene(std::move(select));
}

void PauseMenuScene::onBackToTitleClicked() {
    if (save_service_ && save_service_->isSaving()) {
        setLocalizedMessage("pause.message.save_in_progress", true, {}, "Save in progress");
        return;
    }
    requestReplaceScene(std::make_unique<game::scene::TitleScene>("TitleScene", context_));
}

void PauseMenuScene::adjustMusicVolume(int step) {
    auto& audio = context_.getAudioPlayer();
    const float next = clamp01(audio.getMusicVolume() + static_cast<float>(step) * VOLUME_STEP);
    if (user_settings_service_) {
        user_settings_service_->setMusicVolume(next);
    } else {
        audio.setMusicVolume(next);
    }
    refreshVolumeLabels();
}

void PauseMenuScene::adjustSoundVolume(int step) {
    auto& audio = context_.getAudioPlayer();
    const float next = clamp01(audio.getSoundVolume() + static_cast<float>(step) * VOLUME_STEP);
    if (user_settings_service_) {
        user_settings_service_->setSoundVolume(next);
    } else {
        audio.setSoundVolume(next);
    }
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

    if (user_settings_service_) {
        user_settings_service_->setGlobalTimeScale(scale);
    } else {
        game_time_->time_scale_ = scale;
    }
    refreshTimeScaleLabel();
}

void PauseMenuScene::adjustLanguage(int step) {
    if (!user_settings_service_) {
        return;
    }

    const auto& localization_service = user_settings_service_->localization();
    const auto& languages = localization_service.languages();
    if (languages.empty()) {
        return;
    }

    const std::size_t current = indexOfLanguage(localization_service, user_settings_service_->snapshot().language_tag);
    const std::size_t delta = step > 0 ? 1 : languages.size() - 1;
    const std::size_t next = (current + delta) % languages.size();
    user_settings_service_->setLanguage(languages[next].tag);
    refreshLanguageLabel();
}

} // namespace game::scene
