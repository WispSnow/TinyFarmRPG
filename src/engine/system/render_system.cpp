#include "render_system.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/component/transform_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/render_component.h"
#include "engine/component/layered_sprite_component.h"
#include "engine/component/animation_component.h"
#include "engine/component/tags.h"
#include <entt/entity/registry.hpp>
#include <glm/common.hpp>
#include <algorithm>

namespace engine::system {

void RenderSystem::render(entt::registry& registry, render::Renderer& renderer, const render::Camera& camera, float interpolation_alpha) {
    const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);
    auto view = registry.view<component::RenderComponent, component::TransformComponent, component::SpriteComponent>(
        entt::exclude<component::InvisibleTag>
    );
    bool has_active_layered_entities = false;
    auto layered_probe_view = registry.view<component::LayeredSpriteComponent,
                                            component::RenderComponent,
                                            component::TransformComponent,
                                            component::SpriteComponent>(entt::exclude<component::InvisibleTag>);
    for (auto entity : layered_probe_view) {
        const auto& layered = layered_probe_view.get<component::LayeredSpriteComponent>(entity);
        if (layered.enabled_ && !layered.layers_.empty()) {
            has_active_layered_entities = true;
            break;
        }
    }

    renderer.beginFrame(camera);

    // 快路径：无分层角色时维持原实现（按 RenderComponent 排序后直接绘制，避免额外容器开销）
    if (!has_active_layered_entities) {
        registry.sort<component::RenderComponent>([](const auto& lhs, const auto& rhs) {
            return lhs < rhs;
        });
        view.use<component::RenderComponent>();
        for (auto entity : view) {
            const auto& render = view.get<component::RenderComponent>(entity);
            const auto& transform = view.get<component::TransformComponent>(entity);
            const auto& sprite = view.get<component::SpriteComponent>(entity);
            const glm::vec2 render_position = glm::mix(transform.previous_position_, transform.position_, clamped_alpha);
            auto size = sprite.size_ * transform.scale_;
            auto position = render_position - sprite.pivot_ * size;

            color_options_.start_color = render.color_;
            color_options_.end_color = render.color_;
            color_options_.use_gradient = false;
            transform_options_.rotation_radians = transform.rotation_;
            transform_options_.pivot = sprite.pivot_;
            renderer.drawSprite(sprite.sprite_, position, size, &color_options_, &transform_options_);
        }
        return;
    }

    draw_requests_.clear();
    const std::size_t reserve_count = view.size_hint() * 2;
    if (draw_requests_.capacity() < reserve_count) {
        draw_requests_.reserve(reserve_count);
    }

    for (auto entity : view) {
        const auto& render = view.get<component::RenderComponent>(entity);
        const auto& transform = view.get<component::TransformComponent>(entity);
        const auto& sprite = view.get<component::SpriteComponent>(entity);
        const glm::vec2 render_position = glm::mix(transform.previous_position_, transform.position_, clamped_alpha);
        const glm::vec2 size = sprite.size_ * transform.scale_;
        const glm::vec2 position = render_position - sprite.pivot_ * size;

        engine::utils::TransformOptions draw_transform{};
        draw_transform.rotation_radians = transform.rotation_;
        draw_transform.pivot = sprite.pivot_;

        bool has_layered_draw = false;
        if (const auto* layered = registry.try_get<component::LayeredSpriteComponent>(entity);
            layered && layered->enabled_ && !layered->layers_.empty()) {
            const auto* animation = registry.try_get<component::AnimationComponent>(entity);
            if (animation) {
                for (const auto& layer : layered->layers_) {
                    const auto* layer_layout = layer.resolveLayout(animation->current_animation_id_);
                    if (!layer_layout ||
                        layer_layout->texture_id_ == component::LayeredSpriteLayer::INVALID_TEXTURE_ID) {
                        continue;
                    }
                    if (layer_layout->frame_width_ <= 0.0f || layer_layout->frame_height_ <= 0.0f ||
                        layer_layout->frames_per_direction_ == 0) {
                        continue;
                    }

                    std::size_t source_frame_index = 0;
                    if (!layer_layout->source_frame_index_by_runtime_frame_.empty()) {
                        const std::size_t runtime_index = std::min(animation->current_frame_index_,
                                                                    layer_layout->source_frame_index_by_runtime_frame_.size() - 1);
                        source_frame_index = layer_layout->source_frame_index_by_runtime_frame_[runtime_index];
                    }
                    source_frame_index = std::min(source_frame_index, layer_layout->frames_per_direction_ - 1);
                    const std::size_t atlas_column = layer_layout->direction_block_index_ * layer_layout->frames_per_direction_ +
                                                     source_frame_index;

                    component::Sprite layer_sprite = sprite.sprite_;
                    layer_sprite.texture_id_ = layer_layout->texture_id_;
                    layer_sprite.src_rect_.pos.x = static_cast<float>(atlas_column) * layer_layout->frame_width_;
                    layer_sprite.src_rect_.pos.y = 0.0f;
                    layer_sprite.src_rect_.size.x = layer_layout->frame_width_;
                    layer_sprite.src_rect_.size.y = layer_layout->frame_height_;
                    layer_sprite.is_flipped_ = layer_layout->use_animation_flip_ ? sprite.sprite_.is_flipped_ : false;
                    draw_requests_.push_back(DrawRequest{
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
            draw_requests_.push_back(DrawRequest{
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

    std::sort(draw_requests_.begin(), draw_requests_.end(), [](const DrawRequest& lhs, const DrawRequest& rhs) {
        if (lhs.layer == rhs.layer) {
            return lhs.depth < rhs.depth;
        }
        return lhs.layer < rhs.layer;
    });

    for (const auto& request : draw_requests_) {
        color_options_.start_color = request.color;
        color_options_.end_color = request.color;
        color_options_.use_gradient = false;
        transform_options_ = request.transform;
        renderer.drawSprite(request.sprite, request.position, request.size, &color_options_, &transform_options_);
    }
}

} // namespace engine::system 
