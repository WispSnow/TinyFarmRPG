#pragma once
#include <entt/entt.hpp>
#include <glm/vec2.hpp>

namespace engine::render {
class Renderer;
}

namespace engine::spatial {
class SpatialIndexManager;
}

namespace engine::debug {
class SpatialIndexDebugPanel;
}

namespace engine::system {

/**
 * @brief 负责基于调试面板设置渲染空间索引的可视化覆盖图。
 */
class DebugRenderSystem {
    engine::spatial::SpatialIndexManager& spatial_index_manager_;
    engine::debug::SpatialIndexDebugPanel* spatial_panel_{nullptr};

public:
    DebugRenderSystem(engine::spatial::SpatialIndexManager& spatial_index_manager,
                      engine::debug::SpatialIndexDebugPanel* spatial_panel = nullptr);

    void update(entt::registry& registry, render::Renderer& renderer) const;

private:
    void drawStaticGrid(render::Renderer& renderer) const;
    void drawDynamicGrid(render::Renderer& renderer) const;
    void drawEntityBounds(entt::registry& registry, render::Renderer& renderer) const;
};

} // namespace engine::system
