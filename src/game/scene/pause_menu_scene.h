#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/rmlui/rml_event_bridge.h"

#include <RmlUi/Core/Types.h>

#include <string>
#include <string_view>

namespace engine::core {
enum class State;
}

namespace Rml {
class ElementDocument;
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
    game::save::SaveService* save_service_{nullptr}; // non-owning
    game::data::GameTime* game_time_{nullptr};       // non-owning
    engine::core::State previous_state_{};
    bool close_after_load_{false};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    engine::ui::rmlui::RmlEventBridge event_bridge_{};
    Rml::ElementDocument* document_{nullptr};
    bool click_listener_registered_{false};

    Rml::String message_text_{};
    Rml::String message_color_{"#ff6e6e"};
    bool has_message_{false};

    Rml::String music_text_{"Music 100%"};
    Rml::String sound_text_{"SFX 100%"};
    Rml::String time_scale_text_{"Speed 1.00x"};

    bool save_enabled_{false};
    bool load_enabled_{false};
    bool title_enabled_{true};
    bool time_scale_enabled_{false};

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
    void bindEvents();
    void removeEventListeners();

    void refreshVolumeLabels();
    void refreshTimeScaleLabel();
    void refreshSaveActionButtons();
    void onAsyncSaveCompleted(const game::defs::AsyncSaveCompletedEvent& event);
    void setMessage(std::string message, bool is_error);

    bool onPausePressed();

    void onResumeClicked();
    void onSaveClicked();
    void onLoadClicked();
    void onBackToTitleClicked();

    void adjustMusicVolume(int step);
    void adjustSoundVolume(int step);
    void adjustTimeScale(int step);
};

} // namespace game::scene
