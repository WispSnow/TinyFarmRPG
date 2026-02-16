#include "game_state_debug_panel.h"
#include <imgui.h>
#include <array>
#include <limits>
#include <glm/gtc/type_ptr.hpp>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include "engine/render/camera.h"

namespace engine::debug {

GameStateDebugPanel::GameStateDebugPanel(engine::core::GameState& game_state, engine::render::Camera& camera)
    : game_state_(game_state),
      camera_(camera),
      selected_state_(engine::core::State::Title) {
}

std::string_view GameStateDebugPanel::name() const {
    return "Core: Game State";
}

void GameStateDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Game State Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Current State: %s", stateToString(game_state_.getCurrentState()));
    ImGui::Separator();

    int current = static_cast<int>(selected_state_);
    constexpr engine::core::State states[] = {
        engine::core::State::Title,
        engine::core::State::Playing,
        engine::core::State::Paused,
        engine::core::State::GameOver,
        engine::core::State::LevelClear
    };

    for (int i = 0; i < static_cast<int>(std::size(states)); ++i) {
        const bool was_selected = current == i;
        if (ImGui::RadioButton(stateToString(states[i]), was_selected)) {
            current = i;
            selected_state_ = states[i];
            game_state_.setState(selected_state_);
        }
    }

    ImGui::Separator();
    const auto window_size = game_state_.getWindowSize();
    const auto logical_size = game_state_.getLogicalSize();
    ImGui::Text("Window Size: %.0f x %.0f", window_size.x, window_size.y);
    ImGui::Text("Logical Size: %.0f x %.0f", logical_size.x, logical_size.y);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto position = camera_.getPosition();
        glm::vec2 editable_position = position;
        if (ImGui::DragFloat2("Position", glm::value_ptr(editable_position), 1.0f)) {
            camera_.setPosition(editable_position);
        }

        float zoom = camera_.getZoom();
        const float min_zoom = camera_.getMinZoom();
        const float max_zoom = camera_.getMaxZoom();
        if (ImGui::SliderFloat("Zoom", &zoom, min_zoom, max_zoom, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
            camera_.setZoom(zoom);
        }

        float rotation_deg = glm::degrees(camera_.getRotation());
        if (ImGui::SliderFloat("Rotation (deg)", &rotation_deg, -180.0f, 180.0f)) {
            camera_.setRotation(glm::radians(rotation_deg));
        }

        const auto camera_logical = camera_.getLogicalSize();
        const glm::vec2 view_size = camera_logical / glm::max(camera_.getZoom(), std::numeric_limits<float>::epsilon());
        ImGui::Text("View Size (world): %.1f x %.1f", view_size.x, view_size.y);
    }

    ImGui::End();
}

void GameStateDebugPanel::onShow() {
    syncFromGameState();
}

void GameStateDebugPanel::syncFromGameState() {
    selected_state_ = game_state_.getCurrentState();
}

const char* GameStateDebugPanel::stateToString(engine::core::State state) const {
    switch (state) {
        case engine::core::State::Title: return "Title";
        case engine::core::State::Playing: return "Playing";
        case engine::core::State::Paused: return "Paused";
        case engine::core::State::GameOver: return "Game Over";
        case engine::core::State::LevelClear: return "Level Clear";
        default: return "Unknown";
    }
}

} // namespace engine::debug
