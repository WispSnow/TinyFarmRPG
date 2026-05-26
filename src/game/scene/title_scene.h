#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace game::data {
struct GameTime;
}

namespace game::runtime {
class LocalizationService;
class UserSettingsService;
}

namespace game::defs {
struct LanguageChangedEvent;
}

namespace game::scene {

struct TitleSceneMessage {
    std::string key{};
    std::unordered_map<std::string, std::string> args{};
};

class TitleScene final : public engine::scene::Scene {
    std::shared_ptr<game::data::GameTime> title_game_time_{};
    std::unique_ptr<game::runtime::LocalizationService> localization_service_{};
    std::unique_ptr<game::runtime::UserSettingsService> user_settings_service_{};
    TitleSceneMessage error_message_{};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    bool context_pushed_{false};
    bool runtime_listeners_connected_{false};

    Rml::String error_text_{};
    bool show_error_{false};

public:
    TitleScene(std::string_view name, engine::core::Context& context, TitleSceneMessage error_message = {});
    ~TitleScene() override;

    bool init() override;
    void clean() override;

private:
    void initUserSettings();
    void flushUserSettings();
    void connectRuntimeListeners();
    void disconnectRuntimeListeners();
    [[nodiscard]] const game::runtime::LocalizationService* localization() const noexcept;
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void syncErrorText(bool mark_dirty = true);

    void onStartClicked();
    void onLoadClicked();
    void onMenuClicked();
    void onExitClicked();
    void onLanguageChanged(const game::defs::LanguageChangedEvent& event);
};

} // namespace game::scene
