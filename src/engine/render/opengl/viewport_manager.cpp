// -----------------------------------------------------------------------------
// ViewportManager.cpp
// -----------------------------------------------------------------------------
// 根据窗口尺寸和逻辑渲染尺寸计算带信箱效果的视口（letterboxed viewport）。
// 仅在相关数值发生变化时，标记需要重新计算，从而让 GLRenderer 可以延迟更新视口以优化性能。
// -----------------------------------------------------------------------------
#include "viewport_manager.h"
#include <glad/glad.h>
#include <glm/common.hpp>

namespace engine::render::opengl {

ViewportManager::ViewportManager(const glm::vec2& window_size, const glm::vec2& logical_size) {
    setWindowSize(window_size);
    setLogicalSize(logical_size);
    update();
}

void ViewportManager::setWindowSize(const glm::vec2& window_size) {
    window_size_ = glm::max(glm::vec2(1.0f, 1.0f), window_size);
    dirty_ = true;
}

void ViewportManager::setLogicalSize(const glm::vec2& logical_size) {
    logical_size_ = glm::max(glm::vec2(1.0f, 1.0f), logical_size);
    dirty_ = true;
}

void ViewportManager::update() {
    if (!dirty_) {
        return;
    }

    // 注意：这里的 window_size_ 是 drawable 的像素尺寸（高 DPI 下可能大于 SDL_GetWindowSize()）。
    // glViewport 的单位也是像素，因此整个 letterbox 计算与 viewport 应用都在“像素坐标系”中完成。
    auto metrics = engine::utils::computeLetterboxMetrics(window_size_, logical_size_);
    viewport_ = metrics.viewport;

    // 设置视口
    glViewport(
        static_cast<GLint>(std::round(viewport_.pos.x)), 
        static_cast<GLint>(std::round(viewport_.pos.y)), 
        static_cast<GLsizei>(std::round(viewport_.size.x)), 
        static_cast<GLsizei>(std::round(viewport_.size.y)));

    dirty_ = false;
}

} // namespace engine::render::opengl
