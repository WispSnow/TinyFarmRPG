#include "appearance_system.h"

#include "engine/component/animation_component.h"
#include "engine/component/layered_sprite_component.h"
#include "engine/resource/resource_manager.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/commands.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace game::system {

AppearanceSystem::AppearanceSystem(entt::registry& registry,
                                   entt::dispatcher& dispatcher,
                                   const game::data::AppearanceCatalog& catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      catalog_(catalog) {
    dispatcher_.sink<game::defs::SetAppearanceSlotCommand>().connect<&AppearanceSystem::onSetAppearanceSlotCommand>(this);
    dispatcher_.sink<game::defs::RefreshAppearanceCommand>().connect<&AppearanceSystem::onRefreshAppearanceCommand>(this);
}

AppearanceSystem::~AppearanceSystem() {
    dispatcher_.disconnect(this);
}

void AppearanceSystem::onSetAppearanceSlotCommand(const game::defs::SetAppearanceSlotCommand& command) {
    if (!registry_.valid(command.target)) {
        return;
    }
    if (command.slot.empty()) {
        return;
    }
    if (!catalog_.isRuntimeSwitchableSlot(command.slot)) {
        return;
    }
    if (!command.variant.empty()) {
        const auto& variants = catalog_.variantsForSlot(command.slot);
        if (!variants.empty()) {
            const bool valid_variant = std::find(variants.begin(), variants.end(), command.variant) != variants.end();
            if (!valid_variant) {
                return;
            }
        }
    }

    auto* appearance = registry_.try_get<game::component::AppearanceComponent>(command.target);
    if (!appearance) {
        return;
    }
    appearance->slot_variants_[command.slot] = command.variant;
    appearance->dirty_ = true;
    rebuildLayerCache(command.target);
}

void AppearanceSystem::onRefreshAppearanceCommand(const game::defs::RefreshAppearanceCommand& command) {
    if (!registry_.valid(command.target)) {
        return;
    }
    rebuildLayerCache(command.target);
}

void AppearanceSystem::rebuildLayerCache(entt::entity entity) {
    auto* appearance = registry_.try_get<game::component::AppearanceComponent>(entity);
    auto* layered = registry_.try_get<engine::component::LayeredSpriteComponent>(entity);
    auto* animation = registry_.try_get<engine::component::AnimationComponent>(entity);
    if (!appearance || !layered || !animation) {
        return;
    }
    auto** resource_manager_ptr = registry_.ctx().find<engine::resource::ResourceManager*>();
    engine::resource::ResourceManager* resource_manager = resource_manager_ptr ? *resource_manager_ptr : nullptr;
    std::unordered_set<entt::id_type> ensured_textures{};

    layered->layers_.clear();
    layered->layers_.reserve(catalog_.layerOrder().size());

    for (std::size_t index = 0; index < catalog_.layerOrder().size(); ++index) {
        const auto& slot = catalog_.layerOrder()[index];

        std::string variant = "none";
        if (const auto it = appearance->slot_variants_.find(slot); it != appearance->slot_variants_.end()) {
            variant = it->second;
        }

        engine::component::LayeredSpriteLayer layer{};
        layer.slot_ = slot;
        layer.depth_offset_ = static_cast<float>(index) * engine::component::LayeredSpriteComponent::LAYER_DEPTH_STEP;

        for (const auto& [animation_id, animation_data] : animation->animations_) {
            const auto action_key = catalog_.actionKeyFromAnimationName(animation_data.name_);
            if (!action_key) {
                continue;
            }
            const auto direction_key = catalog_.directionKeyFromAnimationName(animation_data.name_).value_or("down");
            const auto layout_config = catalog_.resolveLayerLayout(*action_key, direction_key);
            if (!layout_config) {
                continue;
            }
            const auto texture = catalog_.resolveLayerTexture(*action_key, slot, variant, appearance->gender_);
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
                        layout.source_frame_index_by_runtime_frame_.push_back(
                            static_cast<std::uint16_t>(clamped_index));
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
