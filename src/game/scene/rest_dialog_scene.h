#pragma once

#include "engine/scene/scene.h"

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

namespace game::scene {

class RestDialogScene final : public engine::scene::Scene {
private:
    engine::core::State previous_state_{};
    int selected_hours_{8};

    engine::ui::UIPanel* dim_{nullptr};
    engine::ui::UIInputBlocker* input_blocker_{nullptr};
    engine::ui::UIPanel* panel_{nullptr};
    engine::ui::UILabel* hours_label_{nullptr};
    engine::ui::UIButton* minus_button_{nullptr};
    engine::ui::UIButton* plus_button_{nullptr};
    engine::ui::UIButton* confirm_button_{nullptr};
    engine::ui::UIButton* cancel_button_{nullptr};

public:
    RestDialogScene(std::string_view name, engine::core::Context& context);
    ~RestDialogScene() override = default;

    bool init() override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void buildLayout();
    void updateHoursLabel();
    void adjustHours(int delta);

    void onConfirm();
    void onCancel();

};

} // namespace game::scene
