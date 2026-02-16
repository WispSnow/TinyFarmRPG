#pragma once

/**
 * @file viewport_manager.h
 * @brief 管理窗口尺寸与游戏逻辑渲染尺寸的转换
 *
 * 此类负责窗口实际尺寸与游戏请求的逻辑渲染尺寸之间的转换，自动计算信箱(letterbox)区域。
 * 当窗口或逻辑尺寸变化时，提供简单接口供渲染器查询当前视口矩形。
 */

#include "engine/utils/math.h"
#include <glm/vec2.hpp>

namespace engine::render::opengl {

class ViewportManager {
    engine::utils::Rect viewport_{};
    glm::vec2 window_size_{};   // window pixel size (SDL_GetWindowSizeInPixels / drawable size)
    glm::vec2 logical_size_{};  // logical render size (fixed render target size)
    bool dirty_{true};

public:
    ViewportManager(const glm::vec2& window_size, const glm::vec2& logical_size);

    void update();  // 更新视口
    bool dirty() const { return dirty_; }

    // setters
    void setWindowSize(const glm::vec2& window_size);
    void setLogicalSize(const glm::vec2& logical_size);

    // getters
    const engine::utils::Rect& getViewport() const { return viewport_; }
    const glm::vec2& getWindowSize() const { return window_size_; }
    const glm::vec2& getLogicalSize() const { return logical_size_; }
};

} // namespace engine::render::opengl
