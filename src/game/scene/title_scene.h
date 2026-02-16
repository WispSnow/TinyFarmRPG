#pragma once

#include "engine/scene/scene.h"

#include <glm/vec2.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace engine::ui {
class UIButton;
class UIImage;
class UILabel;
class UIStackLayout;
} // namespace engine::ui

namespace game::data {
struct GameTime;
} // namespace game::data

namespace game::scene {

class TitleScene final : public engine::scene::Scene {
    engine::ui::UIImage* background_{nullptr};
    engine::ui::UIImage* logo_{nullptr};
    engine::ui::UIStackLayout* button_stack_{nullptr};
    engine::ui::UILabel* error_label_{nullptr};
    std::shared_ptr<game::data::GameTime> title_game_time_{};

    glm::vec2 logo_base_pos_{0.0f, 0.0f};
    float logo_elapsed_{0.0f};

    std::string error_message_{};

public:
    TitleScene(std::string_view name, engine::core::Context& context, std::string error_message = {});
    ~TitleScene() override = default;

    bool init() override;
    void update(float delta_time) override;

private:
    [[nodiscard]] bool initUI();
    void buildLayout();
    void buildBackground();
    void buildLogo();
    void buildButtons();
    void buildMenuButton();
    void buildErrorLabel();

    void onStartClicked();
    void onLoadClicked();
    void onMenuClicked();
    void onExitClicked();
};

} // namespace game::scene
