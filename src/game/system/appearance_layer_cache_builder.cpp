#include "game/system/appearance_layer_cache_builder.h"

#include "engine/component/animation_component.h"
#include "engine/component/layered_sprite_component.h"
#include "engine/resource/resource_manager.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace game::system {

void AppearanceLayerCacheBuilder::rebuild(entt::registry& registry,
                                          entt::entity entity,
                                          const game::data::AppearanceCatalog& catalog,
                                          engine::resource::ResourceManager* resource_manager) {
    auto* appearance = registry.try_get<game::component::AppearanceComponent>(entity);
    auto* layered = registry.try_get<engine::component::LayeredSpriteComponent>(entity);
    auto* animation = registry.try_get<engine::component::AnimationComponent>(entity);
    if (!appearance || !layered || !animation) {
        return;
    }

    std::unordered_set<entt::id_type> ensured_textures{};

    layered->layers_.clear();
    layered->layers_.reserve(catalog.layerOrder().size());

    for (std::size_t index = 0; index < catalog.layerOrder().size(); ++index) {
        const auto& slot = catalog.layerOrder()[index];

        std::string variant = "none";
        if (const auto it = appearance->slot_variants_.find(slot); it != appearance->slot_variants_.end()) {
            variant = it->second;
        }

        engine::component::LayeredSpriteLayer layer{};
        layer.slot_ = slot;
        layer.depth_offset_ = static_cast<float>(index) * engine::component::LayeredSpriteComponent::LAYER_DEPTH_STEP;

        for (const auto& [animation_id, animation_data] : animation->animations_) {
            const auto action_key = catalog.actionKeyFromAnimationName(animation_data.name_);
            if (!action_key) {
                continue;
            }
            const auto direction_key = catalog.directionKeyFromAnimationName(animation_data.name_).value_or("down");
            const auto layout_config = catalog.resolveLayerLayout(*action_key, direction_key);
            if (!layout_config) {
                continue;
            }
            const auto texture = catalog.resolveLayerTexture(*action_key, slot, variant, appearance->gender_);
            if (!texture) {
                continue;
            }

            if (resource_manager && ensured_textures.insert(texture->texture_id_).second) {
                if (!resource_manager->findLoadedTexture(texture->texture_id_)) {
                    // 首次命中未预加载变体时按需补载；后续 rebuild 直接复用缓存纹理。
                    resource_manager->loadTexture(texture->texture_id_, texture->path_);
                }
            }

            engine::component::LayeredAnimationLayout layout{};
            layout.texture_id_ = texture->texture_id_;
            layout.direction_block_index_ = layout_config->direction_block_index_;
            layout.frames_per_direction_ = layout_config->frames_per_direction_;
            layout.use_animation_flip_ = layout_config->use_animation_flip_;

            if (!animation_data.frames_.empty()) {
                const float frame_width = animation_data.frames_.front().src_rect_.size.x;
                const float frame_height = animation_data.frames_.front().src_rect_.size.y;
                layout.frame_width_ = frame_width;
                layout.frame_height_ = frame_height;
                if (frame_width > 0.0f) {
                    float min_source_x = animation_data.frames_.front().src_rect_.pos.x;
                    for (const auto& frame : animation_data.frames_) {
                        min_source_x = std::min(min_source_x, frame.src_rect_.pos.x);
                    }

                    layout.source_frame_index_by_runtime_frame_.reserve(animation_data.frames_.size());
                    const std::size_t max_source_index = (layout.frames_per_direction_ > 0)
                                                             ? (layout.frames_per_direction_ - 1)
                                                             : 0;
                    for (const auto& frame : animation_data.frames_) {
                        const float relative_x = frame.src_rect_.pos.x - min_source_x;
                        long source_index = static_cast<long>(std::lround(relative_x / frame_width));
                        if (source_index < 0) {
                            source_index = 0;
                        }
                        auto clamped_index = static_cast<std::size_t>(source_index);
                        clamped_index = std::min(clamped_index, max_source_index);
                        layout.source_frame_index_by_runtime_frame_.push_back(static_cast<std::uint16_t>(clamped_index));
                    }
                }
            }

            layer.layout_by_animation_id_.insert_or_assign(animation_id, std::move(layout));
        }

        layered->layers_.push_back(std::move(layer));
    }

    appearance->dirty_ = false;
}

} // namespace game::system
