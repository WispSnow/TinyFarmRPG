#pragma once

#include "engine/scene/scene.h"

#include <string>
#include <string_view>

namespace engine::core {
enum class State;
}

namespace engine::ui {
class UIButton;
class UILabel;
class UIPanel;
class UIInputBlocker;
} // namespace engine::ui

namespace game::save {
class SaveService;
}

namespace game::data {
struct GameTime;
}

namespace game::scene {

class PauseMenuScene final : public engine::scene::Scene {
private:
    game::save::SaveService* save_service_{nullptr}; // non-owning
    game::data::GameTime* game_time_{nullptr};       // non-owning
    engine::core::State previous_state_{};
    bool close_after_load_{false};

    engine::ui::UIPanel* dim_{nullptr};
    engine::ui::UIInputBlocker* input_blocker_{nullptr};
    engine::ui::UIPanel* panel_{nullptr};

    engine::ui::UILabel* message_label_{nullptr};

    engine::ui::UIButton* resume_button_{nullptr};
    engine::ui::UIButton* save_button_{nullptr};
    engine::ui::UIButton* load_button_{nullptr};
    engine::ui::UIButton* back_to_title_button_{nullptr};

    engine::ui::UIButton* music_down_button_{nullptr};
    engine::ui::UIButton* music_up_button_{nullptr};
    engine::ui::UILabel* music_label_{nullptr};

    engine::ui::UIButton* sound_down_button_{nullptr};
    engine::ui::UIButton* sound_up_button_{nullptr};
    engine::ui::UILabel* sound_label_{nullptr};

    engine::ui::UIButton* time_scale_down_button_{nullptr};
    engine::ui::UIButton* time_scale_up_button_{nullptr};
    engine::ui::UILabel* time_scale_label_{nullptr};

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
    void buildLayout();
    void refreshVolumeLabels();
    void refreshTimeScaleLabel();
    void setMessage(std::string message, bool is_error);
    void disableButton(engine::ui::UIButton* button);

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
