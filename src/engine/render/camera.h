#pragma once
#include "engine/utils/math.h"
#include <optional>

namespace engine::render {

/**
 * @brief 相机类负责管理相机位置和视口大小，并提供坐标转换功能。
 * 它还包含限制相机移动范围的边界。
 */
class Camera final {
    glm::vec2 position_{0.0f, 0.0f};                                          ///< @brief 相机中心的世界坐标
    float zoom_{1.0f};                                                        ///< @brief 缩放比例
    float rotation_{0.0f};                                                    ///< @brief 旋转角度（单位：弧度）

    glm::vec2 logical_size_{0.0f, 0.0f};                                      ///< @brief 窗口逻辑尺寸
    std::optional<engine::utils::Rect> limit_bounds_{std::nullopt};           ///< @brief 限制相机的移动范围，空值表示不限制
    float smooth_speed_{5.0f};                                                ///< @brief 相机移动的平滑速度
    float min_zoom_{0.1f};                                                    ///< @brief 最小缩放比例
    float max_zoom_{10.0f};                                                   ///< @brief 最大缩放比例

public:
    explicit Camera(glm::vec2 logical_size);
    
    void translate(const glm::vec2& offset);                                ///< @brief 平移相机
    void zoom(float delta);                                                 ///< @brief 缩放相机
    void rotate(float delta);                                               ///< @brief 旋转相机

    glm::vec2 worldToScreen(const glm::vec2& world_pos) const;              ///< @brief 世界坐标转屏幕坐标
    glm::vec2 screenToWorld(const glm::vec2& screen_pos) const;             ///< @brief 屏幕坐标转世界坐标

    /// @brief 世界坐标转屏幕坐标，考虑视差滚动
    glm::vec2 worldToScreenWithParallax(const glm::vec2& world_pos, const glm::vec2& scroll_factor) const;

    // Setters
    void setPosition(glm::vec2 position);
    void setZoom(float zoom);
    void setRotation(float rotation);       // 单位：弧度
    void setLogicalSize(glm::vec2 logical_size);
    void setLimitBounds(std::optional<engine::utils::Rect> limit_bounds);
    void setMinZoom(float min_zoom);
    void setMaxZoom(float max_zoom);

    // Getters
    const glm::vec2& getLogicalSize() const { return logical_size_; }
    const glm::vec2& getPosition() const { return position_; }
    float getZoom() const { return zoom_; }
    float getRotation() const { return rotation_; }       // 单位：弧度
    std::optional<engine::utils::Rect> getLimitBounds() const { return limit_bounds_; }
    float getMinZoom() const { return min_zoom_; }
    float getMaxZoom() const { return max_zoom_; }

    // 禁用拷贝和移动语义
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&) = delete;
    Camera& operator=(Camera&&) = delete;

private:
    void clampPosition();                                                   ///< @brief 限制相机位置在边界内
    void normalizeRotation();                                               ///< @brief 归一化旋转角度(0~2π)
};

} // namespace engine::render
