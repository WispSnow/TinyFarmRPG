#include "debug_render_system.h"
#include "engine/render/renderer.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/spatial/static_tile_grid.h"
#include "engine/spatial/dynamic_entity_grid.h"
#include "engine/debug/panels/spatial_index_debug_panel.h"
#include "engine/component/transform_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include "engine/utils/math.h"
#include <glm/common.hpp>
#include <algorithm>
#include <cmath>

namespace engine::system {
namespace {

[[nodiscard]] engine::utils::ColorOptions makeSolidColor(engine::utils::FColor color) {
    engine::utils::ColorOptions options{};
    options.start_color = color;
    options.end_color = color;
    options.use_gradient = false;
    return options;
}

[[nodiscard]] constexpr int clampIndex(int value, int min_value, int max_value) {
    if (max_value < min_value) {
        return min_value;
    }
    return std::clamp(value, min_value, max_value);
}

[[nodiscard]] constexpr bool isGridValid(const glm::ivec2& size) {
    return size.x > 0 && size.y > 0;
}

[[nodiscard]] constexpr bool isCellSizeValid(const glm::vec2& size) {
    return size.x > 0.0f && size.y > 0.0f;
}

} // namespace

DebugRenderSystem::DebugRenderSystem(engine::spatial::SpatialIndexManager& spatial_index_manager,
                                     engine::debug::SpatialIndexDebugPanel* spatial_panel)
    : spatial_index_manager_(spatial_index_manager), spatial_panel_(spatial_panel) {}

void DebugRenderSystem::update(entt::registry& registry, render::Renderer& renderer) const {
    if (!spatial_panel_ || !spatial_panel_->hasAnyVisualizationEnabled()) {
        return;
    }

    if (spatial_panel_->hasStaticVisualizationEnabled()) {
        drawStaticGrid(renderer);
    }

    if (spatial_panel_->hasDynamicVisualizationEnabled()) {
        drawDynamicGrid(renderer);
        if (spatial_panel_->isEntityBoundsEnabled()) {
            drawEntityBounds(registry, renderer);
        }
    }
}

void DebugRenderSystem::drawStaticGrid(render::Renderer& renderer) const {
    const auto& static_grid = spatial_index_manager_.getStaticGrid();
    const glm::ivec2 map_size = static_grid.getMapSize();
    const glm::vec2 tile_size = glm::vec2(static_grid.getTileSize());

    if (!isGridValid(map_size) || !isCellSizeValid(tile_size)) {
        return;
    }

    const glm::vec2 grid_extent = glm::vec2(map_size) * tile_size;
    const auto view_rect = renderer.getCurrentViewRect().value_or(engine::utils::Rect(glm::vec2(0.0f), grid_extent));

    auto computeIndex = [&](float value, float axis_size) -> int {
        if (axis_size <= 0.0f) {
            return 0;
        }
        return static_cast<int>(std::floor(value / axis_size));
    };

    int min_tile_x = computeIndex(view_rect.pos.x, tile_size.x) - 1;
    int max_tile_x = computeIndex(view_rect.pos.x + view_rect.size.x, tile_size.x) + 1;
    int min_tile_y = computeIndex(view_rect.pos.y, tile_size.y) - 1;
    int max_tile_y = computeIndex(view_rect.pos.y + view_rect.size.y, tile_size.y) + 1;

    min_tile_x = clampIndex(min_tile_x, 0, map_size.x - 1);
    max_tile_x = clampIndex(max_tile_x, 0, map_size.x - 1);
    min_tile_y = clampIndex(min_tile_y, 0, map_size.y - 1);
    max_tile_y = clampIndex(max_tile_y, 0, map_size.y - 1);

    const engine::utils::ColorOptions grid_line_color = makeSolidColor({0.2f, 0.8f, 1.0f, 0.35f});

    if (spatial_panel_->isStaticGridLinesEnabled()) {
        for (int x = min_tile_x; x <= max_tile_x + 1; ++x) {
            const float world_x = glm::clamp(static_cast<float>(x), 0.0f, static_cast<float>(map_size.x)) * tile_size.x;
            renderer.drawLine({world_x, static_cast<float>(min_tile_y) * tile_size.y},
                              {world_x, static_cast<float>(max_tile_y + 1) * tile_size.y},
                              1.0f,
                              &grid_line_color);
        }

        for (int y = min_tile_y; y <= max_tile_y + 1; ++y) {
            const float world_y = glm::clamp(static_cast<float>(y), 0.0f, static_cast<float>(map_size.y)) * tile_size.y;
            renderer.drawLine({static_cast<float>(min_tile_x) * tile_size.x, world_y},
                              {static_cast<float>(max_tile_x + 1) * tile_size.x, world_y},
                              1.0f,
                              &grid_line_color);
        }
    }

    if (spatial_panel_->isThinWallVisualizationEnabled()) {
        const engine::utils::ColorOptions thin_wall_color = makeSolidColor({1.0f, 0.35f, 0.9f, 0.85f});

        for (int y = min_tile_y; y <= max_tile_y; ++y) {
            for (int x = min_tile_x; x <= max_tile_x; ++x) {
                const glm::ivec2 coord{x, y};
                if (static_grid.isSolid(coord)) {
                    continue;
                }

                const auto tile_type = static_grid.getTileType(coord);
                const auto hasFlag = [&](engine::component::TileType flag) {
                    return engine::component::hasTileFlag(tile_type, flag);
                };

                const float left = static_cast<float>(x) * tile_size.x;
                const float right = static_cast<float>(x + 1) * tile_size.x;
                const float top = static_cast<float>(y) * tile_size.y;
                const float bottom = static_cast<float>(y + 1) * tile_size.y;

                if (hasFlag(engine::component::TileType::BLOCK_N)) {
                    renderer.drawLine({left, top}, {right, top}, 2.0f, &thin_wall_color);
                }
                if (hasFlag(engine::component::TileType::BLOCK_S)) {
                    renderer.drawLine({left, bottom}, {right, bottom}, 2.0f, &thin_wall_color);
                }
                if (hasFlag(engine::component::TileType::BLOCK_W)) {
                    renderer.drawLine({left, top}, {left, bottom}, 2.0f, &thin_wall_color);
                }
                if (hasFlag(engine::component::TileType::BLOCK_E)) {
                    renderer.drawLine({right, top}, {right, bottom}, 2.0f, &thin_wall_color);
                }
            }
        }
    }

    if (!spatial_panel_->isHighlightSolidTilesEnabled() &&
        !spatial_panel_->isHighlightStaticEntityCellsEnabled()) {
        return;
    }

    for (int y = min_tile_y; y <= max_tile_y; ++y) {
        for (int x = min_tile_x; x <= max_tile_x; ++x) {
            const glm::ivec2 coord{x, y};
            const glm::vec2 cell_pos = glm::vec2(coord) * tile_size;
            engine::utils::Rect rect(cell_pos, tile_size);

            if (spatial_panel_->isHighlightSolidTilesEnabled() && static_grid.isSolid(coord)) {
                const engine::utils::ColorOptions solid_color = makeSolidColor({1.0f, 0.1f, 0.1f, 0.25f});
                renderer.drawFilledRect(rect, &solid_color);
            }

            if (spatial_panel_->isHighlightStaticEntityCellsEnabled()) {
                const auto& entities = static_grid.getEntities(coord);
                if (!entities.empty()) {
                    engine::utils::FColor base{0.3f, 0.95f, 0.3f, 0.15f};
                    base.a = glm::clamp(0.15f + 0.05f * static_cast<float>(entities.size()), 0.15f, 0.6f);
                    const engine::utils::ColorOptions entity_color = makeSolidColor(base);
                    renderer.drawFilledRect(rect, &entity_color);
                }
            }
        }
    }
}

void DebugRenderSystem::drawDynamicGrid(render::Renderer& renderer) const {
    const auto& dynamic_grid = spatial_index_manager_.getDynamicGrid();
    const glm::ivec2 grid_size = dynamic_grid.getGridSize();
    const glm::vec2 cell_size = dynamic_grid.getCellSize();
    const glm::vec2 world_min = dynamic_grid.getWorldBoundsMin();
    const glm::vec2 world_max = dynamic_grid.getWorldBoundsMax();

    if (!isGridValid(grid_size) || !isCellSizeValid(cell_size)) {
        return;
    }

    engine::utils::Rect fallback_rect(world_min, world_max - world_min);
    const auto view_rect = renderer.getCurrentViewRect().value_or(fallback_rect);

    int min_cell_x = static_cast<int>(std::floor((view_rect.pos.x - world_min.x) / cell_size.x)) - 1;
    int max_cell_x = static_cast<int>(std::ceil((view_rect.pos.x + view_rect.size.x - world_min.x) / cell_size.x)) + 1;
    int min_cell_y = static_cast<int>(std::floor((view_rect.pos.y - world_min.y) / cell_size.y)) - 1;
    int max_cell_y = static_cast<int>(std::ceil((view_rect.pos.y + view_rect.size.y - world_min.y) / cell_size.y)) + 1;

    min_cell_x = clampIndex(min_cell_x, 0, grid_size.x - 1);
    max_cell_x = clampIndex(max_cell_x, 0, grid_size.x - 1);
    min_cell_y = clampIndex(min_cell_y, 0, grid_size.y - 1);
    max_cell_y = clampIndex(max_cell_y, 0, grid_size.y - 1);

    const engine::utils::ColorOptions grid_line_color = makeSolidColor({0.95f, 0.95f, 0.2f, 0.35f});

    if (spatial_panel_->isDynamicGridLinesEnabled()) {
        for (int x = min_cell_x; x <= max_cell_x + 1; ++x) {
            const float world_x = world_min.x + glm::clamp(static_cast<float>(x), 0.0f, static_cast<float>(grid_size.x)) * cell_size.x;
            renderer.drawLine({world_x, world_min.y + static_cast<float>(min_cell_y) * cell_size.y},
                              {world_x, world_min.y + static_cast<float>(max_cell_y + 1) * cell_size.y},
                              1.0f,
                              &grid_line_color);
        }

        for (int y = min_cell_y; y <= max_cell_y + 1; ++y) {
            const float world_y = world_min.y + glm::clamp(static_cast<float>(y), 0.0f, static_cast<float>(grid_size.y)) * cell_size.y;
            renderer.drawLine({world_min.x + static_cast<float>(min_cell_x) * cell_size.x, world_y},
                              {world_min.x + static_cast<float>(max_cell_x + 1) * cell_size.x, world_y},
                              1.0f,
                              &grid_line_color);
        }
    }

    if (spatial_panel_->isHighlightDynamicCellsEnabled()) {
        for (int y = min_cell_y; y <= max_cell_y; ++y) {
            for (int x = min_cell_x; x <= max_cell_x; ++x) {
                const glm::ivec2 cell{x, y};
                const auto& entities = dynamic_grid.getCellEntities(cell);
                if (entities.empty()) {
                    continue;
                }

                engine::utils::FColor base{0.1f, 0.6f, 1.0f, 0.15f};
                base.a = glm::clamp(0.15f + 0.05f * static_cast<float>(entities.size()), 0.15f, 0.7f);
                const engine::utils::ColorOptions cell_color = makeSolidColor(base);

                const glm::vec2 cell_pos = world_min + glm::vec2(cell) * cell_size;
                renderer.drawFilledRect(engine::utils::Rect(cell_pos, cell_size), &cell_color);
            }
        }
    }
}

void DebugRenderSystem::drawEntityBounds(entt::registry& registry, render::Renderer& renderer) const {
    auto view = registry.view<engine::component::SpatialIndexTag, engine::component::TransformComponent>();

    const engine::utils::ColorOptions aabb_color = makeSolidColor({0.2f, 1.0f, 0.2f, 0.9f});
    const engine::utils::ColorOptions circle_color = makeSolidColor({1.0f, 0.65f, 0.0f, 0.9f});

    for (auto entity : view) {
        const auto& transform = view.get<engine::component::TransformComponent>(entity);

        if (registry.all_of<engine::component::AABBCollider>(entity)) {
            const auto& collider = registry.get<engine::component::AABBCollider>(entity);
            const glm::vec2 center = transform.position_ + collider.offset_;
            const glm::vec2 half_size = collider.size_ * 0.5f;
            const glm::vec2 top_left = center - half_size;
            renderer.drawRect(top_left, collider.size_, 1, &aabb_color);
        }

        if (registry.all_of<engine::component::CircleCollider>(entity)) {
            const auto& collider = registry.get<engine::component::CircleCollider>(entity);
            const glm::vec2 center = transform.position_ + collider.offset_;
            renderer.drawCircleOutline(center, collider.radius_, 1.0f, &circle_color);
        }
    }
}

} // namespace engine::system
