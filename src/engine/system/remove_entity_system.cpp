#include "remove_entity_system.h"
#include "engine/component/tags.h"
#include <entt/entity/registry.hpp>
#include <vector>

namespace engine::system {

void RemoveEntitySystem::update(entt::registry& registry) {
    auto view = registry.view<engine::component::NeedRemoveTag>();
    const std::vector<entt::entity> to_destroy(view.begin(), view.end());
    registry.destroy(to_destroy.begin(), to_destroy.end());
}

} // namespace engine::system
