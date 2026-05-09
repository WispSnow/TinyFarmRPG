#pragma once

#include "engine/render/camera.h"

namespace game::runtime {

inline constexpr float GAMEPLAY_CAMERA_DEFAULT_ZOOM = 2.0F;
inline constexpr float GAMEPLAY_CAMERA_MIN_ZOOM = 1.6F;
inline constexpr float GAMEPLAY_CAMERA_MAX_ZOOM = 3.0F;

/// @brief 应用探索态相机缩放范围，供正式游戏与独立工具共享同一套显示基准。
inline void applyGameplayCameraZoomLimits(engine::render::Camera& camera) {
    camera.setMaxZoom(GAMEPLAY_CAMERA_MAX_ZOOM);
    camera.setMinZoom(GAMEPLAY_CAMERA_MIN_ZOOM);
}

/// @brief 应用探索态默认相机缩放。
inline void applyGameplayCameraDefaultZoom(engine::render::Camera& camera) {
    camera.setZoom(GAMEPLAY_CAMERA_DEFAULT_ZOOM);
}

/// @brief 应用完整的探索态相机默认缩放配置。
inline void applyGameplayCameraDefaults(engine::render::Camera& camera) {
    applyGameplayCameraZoomLimits(camera);
    applyGameplayCameraDefaultZoom(camera);
}

} // namespace game::runtime
