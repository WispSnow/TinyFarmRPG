#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/rmlui/rml_event_bridge.h"

#include <RmlUi/Core/Types.h>

#include <memory>
#include <string>
#include <string_view>

namespace engine::core {
enum class State;
}

namespace Rml {
class ElementDocument;
}

namespace engine::ui::rmlui {
class HoverFocusSyncListener;
}

namespace game::save {
class SaveService;
}

namespace game::data {
struct GameTime;
}

namespace game::defs {
struct AsyncSaveCompletedEvent;
}

namespace game::scene {

class PauseMenuScene final : public engine::scene::Scene {
private:
    game::save::SaveService* save_service_{nullptr};
    game::data::GameTime* game_time_{nullptr};
    engine::core::State previous_state_{};
    bool close_after_load_{false};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    engine::ui::rmlui::RmlEventBridge event_bridge_{};
    std::unique_ptr<engine::ui::rmlui::HoverFocusSyncListener> hover_focus_listener_{};
    Rml::ElementDocument* document_{nullptr};

    Rml::String message_text_{};
    Rml::String music_text_{"Music 0%"};
    Rml::String sound_text_{"SFX 0%"};
    Rml::String speed_text_{"Speed 1.00x"};
    bool has_message_{false};
    bool message_is_error_{true};
    bool can_save_{false};
    bool can_load_{false};
    bool can_back_title_{true};

public:
    PauseMenuScene(std::string_view name,
                   engine::core::Context& context,
                   game::save::SaveService* save_service,
                   game::data::GameTime* game_time);
    ~PauseMenuScene() override;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void beforeUnloadOwnedRmlDocuments() override;
    void afterUnloadOwnedRmlDocuments() override;
    void disconnectRuntimeListeners();
    void refreshVolumeLabels();
    void refreshTimeScaleLabel();
    void refreshSaveActionButtons();
    void onAsyncSaveCompleted(const game::defs::AsyncSaveCompletedEvent& event);
    void setMessage(std::string message, bool is_error);

    bool onMenuCancelPressed();

    void onResumeClicked();
    void onSaveClicked();
    void onLoadClicked();
    void onBackToTitleClicked();

    void adjustMusicVolume(int step);
    void adjustSoundVolume(int step);
    void adjustTimeScale(int step);
};

} // namespace game::scene
