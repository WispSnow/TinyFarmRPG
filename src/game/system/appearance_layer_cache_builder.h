#pragma once

#include <entt/entity/fwd.hpp>

namespace engine::resource {
class ResourceManager;
}

namespace game::data {
class AppearanceCatalog;
}

namespace game::system {

/// @brief 无状态外观层缓存构建器，供探索场景和战斗表现层复用。
class AppearanceLayerCacheBuilder final {
public:
    static void rebuild(entt::registry& registry,
                        entt::entity entity,
                        const game::data::AppearanceCatalog& catalog,
                        engine::resource::ResourceManager* resource_manager);
};

} // namespace game::system
