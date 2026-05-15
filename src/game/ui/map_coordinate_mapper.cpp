#include "game/ui/map_coordinate_mapper.h"

#include <algorithm>

namespace game::ui {

MapPreviewLayout computeMapPreviewLayout(const glm::vec2 map_pixel_size, const glm::vec2 frame_size) {
    MapPreviewLayout layout{};
    if (map_pixel_size.x <= 0.0F || map_pixel_size.y <= 0.0F || frame_size.x <= 0.0F || frame_size.y <= 0.0F) {
        return layout;
    }

    layout.scale = std::min(frame_size.x / map_pixel_size.x, frame_size.y / map_pixel_size.y);
    layout.content_size = map_pixel_size * layout.scale;
    layout.content_position = (frame_size - layout.content_size) * 0.5F;
    return layout;
}

glm::vec2 mapLocalPixelToPreviewPoint(const glm::vec2 map_local_pixel_position, const MapPreviewLayout& layout) {
    return layout.content_position + map_local_pixel_position * layout.scale;
}

glm::vec2 mapMarkerBottomCenterTopLeft(const glm::vec2 map_local_pixel_position,
                                       const MapPreviewLayout& layout,
                                       const glm::vec2 frame_size,
                                       const glm::vec2 marker_size) {
    const glm::vec2 preview_point = mapLocalPixelToPreviewPoint(map_local_pixel_position, layout);
    glm::vec2 top_left{
        preview_point.x - marker_size.x * 0.5F,
        preview_point.y - marker_size.y,
    };
    top_left.x = std::clamp(top_left.x, 0.0F, std::max(0.0F, frame_size.x - marker_size.x));
    top_left.y = std::clamp(top_left.y, 0.0F, std::max(0.0F, frame_size.y - marker_size.y));
    return top_left;
}

} // namespace game::ui
