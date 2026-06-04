#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace engine::core {
enum class State;
}

namespace engine::script {
class ScriptHost;
}

namespace game::save {
class SaveService;
}

namespace game::data {
struct GameTime;
}

namespace game::defs {
struct AsyncSaveCompletedEvent;
struct LanguageChangedEvent;
}

namespace game::runtime {
class LocalizationService;
class UserSettingsService;
}

namespace game::scene {

class PauseMenuScene final : public engine::scene::Scene {
private:
    game::save::SaveService* save_service_{nullptr};
    game::data::GameTime* game_time_{nullptr};
    game::runtime::UserSettingsService* user_settings_service_{nullptr};
    engine::script::ScriptHost* script_host_{nullptr};
    engine::core::State previous_state_{};
    bool close_after_load_{false};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};

    Rml::String message_text_{};
    Rml::String music_text_{};
    Rml::String sound_text_{};
    Rml::String speed_text_{};
    Rml::String language_text_{"!options.language!"};
    bool has_message_{false};
    bool message_is_error_{true};
    bool can_save_{false};
    bool can_load_{false};
    bool can_back_title_{true};
    bool can_change_language_{false};
    std::string message_key_{};
    std::unordered_map<std::string, std::string> message_args_{};
    std::string message_fallback_{};

public:
    PauseMenuScene(std::string_view name,
                   engine::core::Context& context,
                   game::save::SaveService* save_service,
                   game::data::GameTime* game_time,
                   game::runtime::UserSettingsService* user_settings_service,
                   engine::script::ScriptHost* script_host);
    ~PauseMenuScene() override;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void disconnectRuntimeListeners();
    void refreshVolumeLabels();
    void refreshTimeScaleLabel();
    void refreshLanguageLabel();
    void refreshSaveActionButtons();
    void refreshLocalizedBindings();
    void onAsyncSaveCompleted(const game::defs::AsyncSaveCompletedEvent& event);
    void onLanguageChanged(const game::defs::LanguageChangedEvent& event);
    [[nodiscard]] const game::runtime::LocalizationService* localization() const noexcept;
    [[nodiscard]] std::string resolveMessageText() const;
    void clearMessage();
    void setLocalizedMessage(std::string_view key,
                             bool is_error,
                             std::unordered_map<std::string, std::string> args = {},
                             std::string fallback = {});
    void publishMessage(std::string message, bool is_error);

    bool onMenuCancelPressed();

    void onResumeClicked();
    void onSaveClicked();
    void onLoadClicked();
    void onBackToTitleClicked();

    void adjustMusicVolume(int step);
    void adjustSoundVolume(int step);
    void adjustTimeScale(int step);
    void adjustLanguage(int step);
};

} // namespace game::scene
