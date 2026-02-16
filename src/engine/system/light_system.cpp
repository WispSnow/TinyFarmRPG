#include "light_system.h"
#include "engine/component/light_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/render/lighting_state.h"
#include "engine/render/renderer.h"
#include "engine/utils/math.h"
#include <entt/entity/registry.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <limits>
#include <ranges>

namespace engine::system {

void LightSystem::update(entt::registry& registry, engine::render::Renderer& renderer) {
    if (const auto* lighting = registry.ctx().find<engine::render::GlobalLightingState>(); lighting) {
        renderer.setAmbient(lighting->ambient);
        for (const auto& directional : lighting->directional_lights
                                       | std::views::values
                                       | std::views::filter([](const auto& d) { return d.enabled; })) {
            renderer.addDirectionalLight(directional.direction, &directional.options);
        }
    } else {
        renderer.setAmbient({1.0f, 1.0f, 1.0f});
    }

    auto point_view = registry.view<engine::component::PointLightComponent, engine::component::TransformComponent>(
        entt::exclude<engine::component::InvisibleTag, engine::component::LightDisabledTag>
    );
    for (auto entity : point_view) {
        const auto& light = point_view.get<engine::component::PointLightComponent>(entity);
        if (light.radius <= 0.0f) {
            continue;
        }
        const auto& transform = point_view.get<engine::component::TransformComponent>(entity);
        renderer.addPointLight(transform.position_ + light.offset, light.radius, &light.options);
    }

    auto spot_view = registry.view<engine::component::SpotLightComponent, engine::component::TransformComponent>(
        entt::exclude<engine::component::InvisibleTag, engine::component::LightDisabledTag>
    );
    for (auto entity : spot_view) {
        const auto& light = spot_view.get<engine::component::SpotLightComponent>(entity);
        if (light.radius <= 0.0f) {
            continue;
        }

        glm::vec2 direction = light.direction;
        if (glm::length(direction) <= std::numeric_limits<float>::epsilon()) {
            direction = {1.0f, 0.0f};
        } else {
            direction = glm::normalize(direction);
        }

        const auto& transform = spot_view.get<engine::component::TransformComponent>(entity);
        renderer.addSpotLight(transform.position_, light.radius, direction, &light.options);
    }

    auto emissive_rect_view = registry.view<engine::component::EmissiveRectComponent, engine::component::TransformComponent>(
        entt::exclude<engine::component::InvisibleTag, engine::component::LightDisabledTag>
    );
    for (auto entity : emissive_rect_view) {
        const auto& emissive = emissive_rect_view.get<engine::component::EmissiveRectComponent>(entity);
        if (emissive.size.x <= 0.0f || emissive.size.y <= 0.0f) {
            continue;
        }
        const auto& transform = emissive_rect_view.get<engine::component::TransformComponent>(entity);
        renderer.addEmissiveRect(engine::utils::Rect(transform.position_, emissive.size), &emissive.params);
    }

    auto emissive_sprite_view = registry.view<
        engine::component::EmissiveSpriteComponent,
        engine::component::TransformComponent,
        engine::component::SpriteComponent
    >(entt::exclude<engine::component::InvisibleTag, engine::component::LightDisabledTag>);
    for (auto entity : emissive_sprite_view) {
        const auto& emissive = emissive_sprite_view.get<engine::component::EmissiveSpriteComponent>(entity);
        const auto& transform = emissive_sprite_view.get<engine::component::TransformComponent>(entity);
        const auto& sprite = emissive_sprite_view.get<engine::component::SpriteComponent>(entity);

        const glm::vec2 size = sprite.size_ * transform.scale_;
        if (size.x <= 0.0f || size.y <= 0.0f) {
            continue;
        }
        const glm::vec2 position = transform.position_ - sprite.pivot_ * size;

        engine::utils::EmissiveParams params = emissive.params;
        params.transform.rotation_radians = transform.rotation_;
        params.transform.pivot = sprite.pivot_;

        renderer.addEmissiveSprite(sprite.sprite_, position, size, &params);
    }
}

} // namespace engine::system
