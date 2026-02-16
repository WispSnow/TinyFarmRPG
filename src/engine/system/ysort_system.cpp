#include "ysort_system.h"
#include "engine/component/render_component.h"
#include "engine/component/transform_component.h"
#include <entt/entity/registry.hpp>

namespace engine::system {

void YSortSystem::update(entt::registry& registry) {
    // 让 RenderComponent.depth_ 等于 TransformComponent.position_.y（y 越大越靠下 → 越晚绘制 → 越遮挡在前）。
    auto view = registry.view<component::RenderComponent, const component::TransformComponent>();
    for (auto entity : view) {
        auto& render = view.get<component::RenderComponent>(entity);
        const auto& transform = view.get<const component::TransformComponent>(entity);
        render.depth_ = transform.position_.y;
    }
}

}
