#include "render_system.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/component/transform_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/render_component.h"
#include "engine/component/layered_sprite_component.h"
#include "engine/component/animation_component.h"
#include "engine/component/tags.h"
#include <spdlog/spdlog.h>
#include <entt/entity/registry.hpp>
#include <glm/common.hpp>
#include <algorithm>
#include <vector>

namespace engine::system {

void RenderSystem::render(entt::registry& registry, render::Renderer& renderer, const render::Camera& camera, float interpolation_alpha) {
    const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);
    struct DrawRequest {
        int layer{0};
        float depth{0.0f};
        component::Sprite sprite{};
        glm::vec2 position{0.0f, 0.0f};
        glm::vec2 size{0.0f, 0.0f};
        engine::utils::FColor color{engine::utils::FColor::white()};
        engine::utils::TransformOptions transform{};
    };
    std::vector<DrawRequest> draw_requests{};

    // 对RenderComponent进行排序 (需要自定义RenderComponent的比较运算符)
    registry.sort<component::RenderComponent>([](const auto& lhs, const auto& rhs) {
        return lhs < rhs;
    });

    // 执行渲染：EnTT view 默认会选择“最小”的 storage 进行迭代，因此这里强制使用已排序的 RenderComponent storage。
    renderer.beginFrame(camera);
    auto view = registry.view<component::RenderComponent, component::TransformComponent, component::SpriteComponent>(
        entt::exclude<component::InvisibleTag>
    );
    view.use<component::RenderComponent>();
    draw_requests.reserve(view.size_hint() * 2);
    for (auto entity : view) {
        const auto& render = view.get<component::RenderComponent>(entity);
        const auto& transform = view.get<component::TransformComponent>(entity);
        const auto& sprite = view.get<component::SpriteComponent>(entity);
        const glm::vec2 render_position = glm::mix(transform.previous_position_, transform.position_, clamped_alpha);
        auto size = sprite.size_ * transform.scale_;                    // 大小 = 精灵的大小 * 变换组件的缩放
        auto position = render_position - sprite.pivot_ * size;         // 位置 = 插值后位置 - 精灵的锚点 * 大小

        engine::utils::TransformOptions draw_transform{};
        draw_transform.rotation_radians = transform.rotation_;
        draw_transform.pivot = sprite.pivot_;

        bool has_layered_draw = false;
        if (const auto* layered = registry.try_get<component::LayeredSpriteComponent>(entity);
            layered && layered->enabled_ && !layered->layers_.empty()) {
            const auto* animation = registry.try_get<component::AnimationComponent>(entity);
            if (animation) {
                for (const auto& layer : layered->layers_) {
                    const entt::id_type layer_texture_id = layer.resolveTexture(animation->current_animation_id_);
                    if (layer_texture_id == entt::null) {
                        continue;
                    }

                    component::Sprite layer_sprite = sprite.sprite_;
                    layer_sprite.texture_id_ = layer_texture_id;
                    draw_requests.push_back(DrawRequest{
                        render.layer_,
                        render.depth_ + layer.depth_offset_,
                        std::move(layer_sprite),
                        position,
                        size,
                        render.color_,
                        draw_transform
                    });
                    has_layered_draw = true;
                }
            }
        }

        if (!has_layered_draw) {
            draw_requests.push_back(DrawRequest{
                render.layer_,
                render.depth_,
                sprite.sprite_,
                position,
                size,
                render.color_,
                draw_transform
            });
        }
    }

    std::sort(draw_requests.begin(), draw_requests.end(), [](const DrawRequest& lhs, const DrawRequest& rhs) {
        if (lhs.layer == rhs.layer) {
            return lhs.depth < rhs.depth;
        }
        return lhs.layer < rhs.layer;
    });

    for (const auto& request : draw_requests) {
        color_options_.start_color = request.color;
        color_options_.end_color = request.color;
        color_options_.use_gradient = false;
        transform_options_ = request.transform;
        renderer.drawSprite(request.sprite, request.position, request.size, &color_options_, &transform_options_);
    }
}

} // namespace engine::system 
