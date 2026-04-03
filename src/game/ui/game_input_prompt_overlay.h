#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"

#include <RmlUi/Core/Types.h>

#include <cstdint>

namespace engine::input {
class InputManager;
}

namespace engine::ui::rmlui {
class RmlUiRuntime;
}

namespace game::ui {

/**
 * @brief 游戏内输入提示 overlay
 *
 * 独立负责读取当前输入设备对应的按键提示，并将其渲染到 game_overlay.rml。
 */
class GameInputPromptOverlay final {
public:
    GameInputPromptOverlay(engine::ui::rmlui::RmlUiRuntime& runtime,
                           engine::input::InputManager& input_manager,
                           uint64_t owner_scene_id);
    ~GameInputPromptOverlay();

    GameInputPromptOverlay(const GameInputPromptOverlay&) = delete;
    GameInputPromptOverlay& operator=(const GameInputPromptOverlay&) = delete;
    GameInputPromptOverlay(GameInputPromptOverlay&&) = delete;
    GameInputPromptOverlay& operator=(GameInputPromptOverlay&&) = delete;

    void update();
    void setVisible(bool visible);
    void toggleVisible();

    [[nodiscard]] bool isVisible() const { return visible_; }
    [[nodiscard]] bool isReady() const { return document_controller_.isModelValid(); }

private:
    void refreshPromptTexts();

    engine::input::InputManager& input_manager_;
    engine::ui::rmlui::RmlDocumentController document_controller_{};

    Rml::String primary_prompt_text_{"-"};
    Rml::String secondary_prompt_text_{"-"};
    Rml::String inventory_prompt_text_{"-"};
    Rml::String pause_prompt_text_{"-"};
    Rml::String toggle_prompt_text_{"-"};
    bool visible_{false};
};

} // namespace game::ui
