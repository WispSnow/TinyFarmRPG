#pragma once

#include <glm/vec2.hpp>

namespace game::ui {

struct MapPreviewLayout {
    glm::vec2 content_position{};
    glm::vec2 content_size{};
    float scale{1.0F};
};

[[nodiscard]] MapPreviewLayout computeMapPreviewLayout(glm::vec2 map_pixel_size, glm::vec2 frame_size);
[[nodiscard]] glm::vec2 mapLocalPixelToPreviewPoint(glm::vec2 map_local_pixel_position,
                                                    const MapPreviewLayout& layout);
[[nodiscard]] glm::vec2 mapMarkerTopLeft(glm::vec2 map_local_pixel_position,
                                         const MapPreviewLayout& layout,
                                         glm::vec2 marker_size);

} // namespace game::ui
