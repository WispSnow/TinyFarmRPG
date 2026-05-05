#include "appearance_system.h"

#include "engine/resource/resource_manager.h"
#include "game/component/appearance_component.h"
#include "game/data/appearance_catalog.h"
#include "game/defs/commands.h"
#include "game/system/appearance_layer_cache_builder.h"

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
    auto** resource_manager_ptr = registry_.ctx().find<engine::resource::ResourceManager*>();
    engine::resource::ResourceManager* resource_manager = resource_manager_ptr ? *resource_manager_ptr : nullptr;
    AppearanceLayerCacheBuilder::rebuild(registry_, entity, catalog_, resource_manager);
}

} // namespace game::system
