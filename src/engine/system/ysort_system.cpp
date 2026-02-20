#include "ysort_system.h"
#include "engine/component/render_component.h"
#include "engine/component/transform_component.h"
#include <entt/entity/registry.hpp>
#include <glm/common.hpp>
#include <algorithm>

namespace engine::system {

void YSortSystem::render(entt::registry& registry, float interpolation_alpha) {
    const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);

    // 让 RenderComponent.depth_ 等于 TransformComponent.position_.y（y 越大越靠下 → 越晚绘制 → 越遮挡在前）。
    auto view = registry.view<component::RenderComponent, const component::TransformComponent>();
    for (auto entity : view) {
        auto& render = view.get<component::RenderComponent>(entity);
        const auto& transform = view.get<const component::TransformComponent>(entity);
        const glm::vec2 render_position = glm::mix(transform.previous_position_, transform.position_, clamped_alpha);
        render.depth_ = render_position.y;
    }
}

}
