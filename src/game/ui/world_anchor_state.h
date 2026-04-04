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

/**
 * @brief 管理 UI 元素的世界锚点状态，并支持在渲染插值阶段解析到屏幕坐标。
 *
 * 当 UI 需要跟随场景中的实体或世界坐标移动时，调用方可写入当前世界锚点与屏幕偏移；
 * 状态对象会同时保留上一份锚点，用于在渲染阶段按插值系数平滑过渡，减少镜头或实体移动时的抖动。
 */
class WorldAnchorState final {
public:
    /// @brief 切换到世界锚点模式，并记录本次更新前的锚点以供插值。
    /// @param world_pos 当前逻辑帧对应的世界坐标锚点。
    /// @param screen_offset 叠加在最终屏幕坐标上的像素偏移，常用于气泡抬高显示。
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

    /// @brief 清除世界锚点并回到纯屏幕定位模式。
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

    /// @brief 按插值系数在上一锚点与当前锚点之间取值。
    /// @param interpolation_alpha 渲染插值系数，超出范围时会被钳制到 `[0, 1]`。
    /// @return 插值后的世界坐标。
    [[nodiscard]] glm::vec2 interpolateWorldAnchor(float interpolation_alpha) const {
        const float clamped_alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
        return glm::mix(previous_world_anchor_, world_anchor_, clamped_alpha);
    }

    /// @brief 将插值后的世界锚点通过相机投影为屏幕坐标，并叠加屏幕偏移。
    /// @param camera 用于执行世界坐标到屏幕坐标转换的相机。
    /// @param interpolation_alpha 渲染插值系数。
    /// @return 最终可用于 UI 布局的屏幕坐标。
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
