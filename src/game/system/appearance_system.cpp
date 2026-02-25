#include "appearance_system.h"

#include "engine/component/animation_component.h"
#include "engine/component/layered_sprite_component.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/commands.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <algorithm>

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
            const auto texture = catalog_.resolveLayerTexture(*action_key, slot, variant, appearance->gender_);
            if (!texture) {
                continue;
            }
            layer.texture_by_animation_id_.insert_or_assign(animation_id, texture->texture_id_);
        }

        layered->layers_.push_back(std::move(layer));
    }

    appearance->dirty_ = false;
}

} // namespace game::system
