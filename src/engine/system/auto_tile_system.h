#pragma once

#include <glm/vec2.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include "engine/utils/events.h"

namespace engine::component {
struct AutoTileComponent;
}

namespace engine::resource {
class AutoTileLibrary;
}

namespace engine::spatial {
class SpatialIndexManager;
}

namespace engine::system {

/**
 * @brief 自动图块系统
 *
 * 负责根据 AutoTileDirtyTag 刷新中心及邻居瓦片的贴图。
 */
class AutoTileSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    engine::resource::AutoTileLibrary& auto_tile_library_;
    engine::spatial::SpatialIndexManager& spatial_index_;

public:
    AutoTileSystem(entt::registry& registry,
                   entt::dispatcher& dispatcher,
                   engine::resource::AutoTileLibrary& auto_tile_library,
                   engine::spatial::SpatialIndexManager& spatial_index);
    ~AutoTileSystem();

    void update(); ///< @brief 执行一次自动图块刷新

private:
    void refreshEntity(entt::entity entity,
                       glm::ivec2 tile_coord,
                       engine::component::AutoTileComponent& auto_tile);
    [[nodiscard]] uint8_t computeMask(glm::ivec2 tile_coord, entt::id_type rule_id) const;
    [[nodiscard]] bool hasMatchingRule(glm::ivec2 tile_coord, entt::id_type rule_id) const;
    [[nodiscard]] glm::ivec2 worldToTileCoord(glm::vec2 position) const;
    [[nodiscard]] bool isValidCoord(glm::ivec2 coord) const;
    void markClusterDirty(glm::ivec2 tile_coord, entt::id_type rule_id, bool include_center);
    void markTileDirty(glm::ivec2 tile_coord, entt::id_type rule_id);

    void onAddAutoTileEntity(const engine::utils::AddAutoTileEntityEvent& event);
    void onRemoveAutoTileEntity(const engine::utils::RemoveAutoTileEntityEvent& event);
};

} // namespace engine::system
