#pragma once

#include "engine/render/camera.h"

#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <cstdint>

namespace game::ui {

enum class WorldAnchorMode : std::uint8_t {
    Screen,
    WorldAnchor,
};

class WorldAnchorState final {
public:
    void setWorldAnchor(glm::vec2 world_pos, glm::vec2 screen_offset = {0.0F, 0.0F}) {
        if (mode_ != WorldAnchorMode::WorldAnchor) {
            previous_world_anchor_ = world_pos;
        } else {
            previous_world_anchor_ = world_anchor_;
        }

        mode_ = WorldAnchorMode::WorldAnchor;
        world_anchor_ = world_pos;
        screen_offset_ = screen_offset;
    }

    void clearWorldAnchor() {
        mode_ = WorldAnchorMode::Screen;
        world_anchor_ = {0.0F, 0.0F};
        previous_world_anchor_ = {0.0F, 0.0F};
        screen_offset_ = {0.0F, 0.0F};
    }

    [[nodiscard]] bool hasWorldAnchor() const { return mode_ == WorldAnchorMode::WorldAnchor; }
    [[nodiscard]] WorldAnchorMode mode() const { return mode_; }
    [[nodiscard]] const glm::vec2& worldAnchor() const { return world_anchor_; }
    [[nodiscard]] const glm::vec2& previousWorldAnchor() const { return previous_world_anchor_; }
    [[nodiscard]] const glm::vec2& screenOffset() const { return screen_offset_; }

    [[nodiscard]] glm::vec2 interpolateWorldAnchor(float interpolation_alpha) const {
        const float clamped_alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
        return glm::mix(previous_world_anchor_, world_anchor_, clamped_alpha);
    }

    [[nodiscard]] glm::vec2 resolveScreenAnchorPosition(const engine::render::Camera& camera,
                                                        float interpolation_alpha) const {
        return camera.worldToScreen(interpolateWorldAnchor(interpolation_alpha)) + screen_offset_;
    }

private:
    WorldAnchorMode mode_{WorldAnchorMode::Screen};
    glm::vec2 world_anchor_{0.0F, 0.0F};
    glm::vec2 previous_world_anchor_{0.0F, 0.0F};
    glm::vec2 screen_offset_{0.0F, 0.0F};
};

} // namespace game::ui
