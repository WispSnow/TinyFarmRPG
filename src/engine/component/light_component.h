#pragma once
#include "engine/utils/defs.h"
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace engine::component {

/// 点光源组件 —— LightSystem 提交到 LightingPass
struct PointLightComponent {
    float radius{0.0f};
    glm::vec2 offset{0.0f, 0.0f};
    engine::utils::PointLightOptions options{};
};

/// 聚光灯组件 —— LightSystem 提交到 LightingPass
struct SpotLightComponent {
    float radius{0.0f};
    glm::vec2 direction{1.0f, 0.0f};
    engine::utils::SpotLightOptions options{};
};

/// 矩形自发光组件 —— LightSystem 提交到 EmissivePass
struct EmissiveRectComponent {
    glm::vec2 size{0.0f};
    engine::utils::EmissiveParams params{};
};

/// 精灵自发光组件 —— LightSystem 提交到 EmissivePass
struct EmissiveSpriteComponent {
    engine::utils::EmissiveParams params{};
};

} // namespace engine::component
