#include "game/runtime/user_settings_service.h"

#include "engine/platform/filesystem_paths.h"

#include "game/data/game_time.h"
#include "game/defs/options_events.h"
#include "game/runtime/localization_service.h"
#include "game/ui/rml_localization_applier.h"

#include "engine/audio/audio_player.h"
#include "engine/render/text_renderer.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/ElementDocument.h>
#include <entt/signal/dispatcher.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace game::runtime {

UserSettingsService::UserSettingsService(entt::dispatcher& dispatcher,
                                         engine::audio::AudioPlayer& audio_player,
                                         game::data::GameTime& game_time,
                                         LocalizationService& localization,
                                         engine::render::TextRenderer& text_renderer,
                                         engine::ui::rmlui::RmlUiRuntime& rml_runtime)
    : dispatcher_(dispatcher),
      audio_player_(audio_player),
      game_time_(game_time),
      localization_(localization),
      text_renderer_(text_renderer),
      rml_runtime_(rml_runtime) {
    rml_runtime_.setDocumentLoadedCallback([this](Rml::ElementDocument& document, uint64_t, const std::string&) {
        game::ui::applyRmlLocalization(document, localization_);
    });
}

UserSettingsService::~UserSettingsService() noexcept {
    rml_runtime_.setDocumentLoadedCallback({});
}

bool UserSettingsService::loadFromFile(std::string_view path) {
    const std::filesystem::path fs_path{path};
    std::ifstream file(fs_path);
    if (!file.is_open()) {
        return false;
    }

    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const nlohmann::json root = nlohmann::json::parse(content, nullptr, false);
    if (root.is_discarded()) {
        spdlog::warn("UserSettingsService: 解析配置 '{}' 失败，已忽略。", fs_path.string());
        return false;
    }

    if (!parseUserSettingsJson(root, settings_)) {
        return false;
    }
    settings_.language_tag = localization_.resolveSupportedLanguageTag(settings_.language_tag);
    spdlog::info("UserSettingsService: 已加载 '{}'", fs_path.string());
    return true;
}

bool UserSettingsService::loadFromFileOrFallback(std::string_view user_path, std::string_view default_path) {
    const std::filesystem::path user_override_path = engine::platform::userOverridePathFor(user_path);
    if (loadFromFile(user_override_path.string())) {
        return true;
    }
    if (loadFromFile(default_path)) {
        spdlog::info("UserSettingsService: 用户偏好缺失，已退回默认模板 '{}'。", default_path);
        return true;
    }
    spdlog::info("UserSettingsService: 未找到任何偏好配置，使用代码内默认值。");
    return false;
}

bool UserSettingsService::saveToFile(std::string_view path) const {
    const std::filesystem::path fs_path = engine::platform::writableConfigPath(path);
    if (auto parent = fs_path.parent_path(); !parent.empty()) {
        std::error_code ec{};
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream file(fs_path);
    if (!file.is_open()) {
        spdlog::warn("UserSettingsService: 无法写入偏好配置 '{}'", fs_path.string());
        return false;
    }
    file << serializeUserSettings(settings_).dump(2);
    return true;
}

bool UserSettingsService::flushIfDirty(std::string_view path) {
    if (!dirty_) {
        return true;
    }
    if (!saveToFile(path)) {
        return false;
    }
    dirty_ = false;
    return true;
}

void UserSettingsService::applyAll() {
    applyAudio();
    applyTimeScale();
    applyUiFontScale();
    (void)applyLanguage();
}

void UserSettingsService::applyAudio() {
    audio_player_.setMusicVolume(settings_.music_volume);
    audio_player_.setSoundVolume(settings_.sound_volume);
}

void UserSettingsService::applyTimeScale() {
    game_time_.time_scale_ = settings_.global_time_scale;
}

void UserSettingsService::applyUiFontScale() {
    rml_runtime_.applyBodyFontScaleClassToAllDocuments(uiFontScaleClassName(settings_.ui_font_scale));
}

bool UserSettingsService::applyLanguage() {
    if (!localization_.setLanguage(settings_.language_tag)) {
        settings_.language_tag = std::string{localization_.currentLanguageTag()};
        text_renderer_.setDefaultLanguage(settings_.language_tag);
        applyRmlLocalizationToAllDocuments();
        return false;
    }
    settings_.language_tag = std::string{localization_.currentLanguageTag()};
    text_renderer_.setDefaultLanguage(settings_.language_tag);
    applyRmlLocalizationToAllDocuments();
    return true;
}

void UserSettingsService::applyRmlLocalizationToAllDocuments() {
    rml_runtime_.forEachDocument([this](Rml::ElementDocument& document, uint64_t, const std::string&) {
        game::ui::applyRmlLocalization(document, localization_);
    });
}

void UserSettingsService::setMusicVolume(float value) {
    settings_.music_volume = std::clamp(value, 0.0f, 1.0f);
    applyAudio();
    dirty_ = true;
}

void UserSettingsService::setSoundVolume(float value) {
    settings_.sound_volume = std::clamp(value, 0.0f, 1.0f);
    applyAudio();
    dirty_ = true;
}

void UserSettingsService::setGlobalTimeScale(float value) {
    if (!std::isfinite(value) || value <= 0.0f) {
        value = 1.0f;
    }
    settings_.global_time_scale = std::clamp(value, GLOBAL_TIME_SCALE_MIN, GLOBAL_TIME_SCALE_MAX);
    applyTimeScale();
    dirty_ = true;
}

void UserSettingsService::setBattleAnimationSpeed(float value) {
    const float clamped = clampToNearestSpeedChoice(value);
    if (settings_.battle_animation_speed == clamped) {
        return;
    }
    settings_.battle_animation_speed = clamped;
    dirty_ = true;
    dispatcher_.trigger(game::defs::BattleAnimationSpeedChangedEvent{clamped});
}

void UserSettingsService::setShowDamagePopup(bool visible) {
    if (settings_.show_damage_popup == visible) {
        return;
    }
    settings_.show_damage_popup = visible;
    dirty_ = true;
    dispatcher_.trigger(game::defs::DamagePopupVisibilityChangedEvent{visible});
}

void UserSettingsService::setShowEnemyHpBar(bool visible) {
    if (settings_.show_enemy_hp_bar == visible) {
        return;
    }
    settings_.show_enemy_hp_bar = visible;
    dirty_ = true;
    dispatcher_.trigger(game::defs::EnemyHpBarVisibilityChangedEvent{visible});
}

void UserSettingsService::setCursorMemory(bool enabled) {
    if (settings_.cursor_memory == enabled) {
        return;
    }
    settings_.cursor_memory = enabled;
    dirty_ = true;
    dispatcher_.trigger(game::defs::CursorMemoryChangedEvent{enabled});
}

void UserSettingsService::setUiFontScale(UiFontScale scale) {
    if (settings_.ui_font_scale == scale) {
        return;
    }
    settings_.ui_font_scale = scale;
    dirty_ = true;
    applyUiFontScale();
    dispatcher_.trigger(game::defs::UiFontScaleChangedEvent{scale});
}

void UserSettingsService::setLanguage(std::string_view language_tag) {
    const std::string old_language = settings_.language_tag;
    const std::string next_language = localization_.resolveSupportedLanguageTag(language_tag);
    if (old_language == next_language) {
        return;
    }

    settings_.language_tag = next_language;
    const bool applied = applyLanguage();
    if (!applied && settings_.language_tag == old_language) {
        return;
    }
    if (settings_.language_tag != old_language) {
        dirty_ = true;
        dispatcher_.trigger(game::defs::LanguageChangedEvent{settings_.language_tag});
    }
}

} // namespace game::runtime
