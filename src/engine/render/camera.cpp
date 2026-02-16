#include "camera.h"
#include "engine/utils/math.h"
#include <spdlog/spdlog.h>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

namespace engine::render {

Camera::Camera(glm::vec2 logical_size) : logical_size_(logical_size) {}

glm::vec2 Camera::worldToScreen(const glm::vec2& world_pos) const {
    if (zoom_ <= 0.0f) {
        return world_pos;
    }
    const glm::vec2 offset = (world_pos - position_) * zoom_;
    return offset + logical_size_ * 0.5f;
}

glm::vec2 Camera::worldToScreenWithParallax(const glm::vec2 &world_pos, const glm::vec2 &scroll_factor) const
{
    if (zoom_ <= 0.0f) {
        return world_pos;
    }
    const glm::vec2 effective_position = position_ * scroll_factor;
    const glm::vec2 offset = (world_pos - effective_position) * zoom_;
    return offset + logical_size_ * 0.5f;
}

glm::vec2 Camera::screenToWorld(const glm::vec2 &screen_pos) const
{
    if (zoom_ <= 0.0f) {
        return screen_pos;
    }
    const glm::vec2 centered = screen_pos - logical_size_ * 0.5f;
    return centered / zoom_ + position_;
}

void Camera::setPosition(glm::vec2 position) {
    position_ = position;
    clampPosition();
}

void Camera::setZoom(float zoom) {
    zoom_ = glm::clamp(zoom, min_zoom_, max_zoom_);
}

void Camera::setRotation(float rotation) {
    rotation_ = rotation;
    normalizeRotation();
}

void Camera::setLogicalSize(glm::vec2 logical_size) {
    logical_size_ = logical_size;
    clampPosition();
}

void Camera::translate(const glm::vec2 &offset)
{
    position_ += offset;
    clampPosition();
}

void Camera::zoom(float delta) {
    setZoom(zoom_ + delta);
}

void Camera::rotate(float delta) {
    setRotation(rotation_ + delta);
}

void Camera::setLimitBounds(std::optional<engine::utils::Rect> limit_bounds)
{
    limit_bounds_ = std::move(limit_bounds);
    clampPosition(); // 设置边界后，立即应用限制
}

void Camera::setMinZoom(float min_zoom) {
    min_zoom_ = min_zoom;
    setZoom(zoom_);
}

void Camera::setMaxZoom(float max_zoom) {
    max_zoom_ = max_zoom;
    setZoom(zoom_);
}

void Camera::clampPosition()
{
    // 边界检查需要确保相机视图（position 到 position + logical_size_）在 limit_bounds 内
    if (limit_bounds_.has_value() && limit_bounds_->size.x > 0 && limit_bounds_->size.y > 0 && zoom_ > 0.0f) {
        glm::vec2 view_size = logical_size_ / zoom_;
        glm::vec2 half_size = view_size * 0.5f;

        glm::vec2 min_cam_pos = limit_bounds_->pos + half_size;
        glm::vec2 max_cam_pos = limit_bounds_->pos + limit_bounds_->size - half_size;

        // 如果视口比限制区域大，定位到限制区域中心
        if (max_cam_pos.x < min_cam_pos.x) {
            float center_x = limit_bounds_->pos.x + limit_bounds_->size.x * 0.5f;
            min_cam_pos.x = max_cam_pos.x = center_x;
        }
        if (max_cam_pos.y < min_cam_pos.y) {
            float center_y = limit_bounds_->pos.y + limit_bounds_->size.y * 0.5f;
            min_cam_pos.y = max_cam_pos.y = center_y;
        }

        position_ = glm::clamp(position_, min_cam_pos, max_cam_pos);
    }
    // 如果 limit_bounds 无效则不进行限制
}

void Camera::normalizeRotation() {
    rotation_ = glm::mod(rotation_, glm::two_pi<float>());
}

} // namespace engine::render 
