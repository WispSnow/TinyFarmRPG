#pragma once

#include "engine/resource/decoded_image.h"

#include <glm/vec2.hpp>

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

namespace game::ui {

struct MapPreviewBuildResult {
    engine::resource::DecodedImage image{};
    glm::ivec2 map_pixel_size{};
    bool from_cache{false};

    [[nodiscard]] bool valid() const noexcept { return image.valid() && map_pixel_size.x > 0 && map_pixel_size.y > 0; }
};

/// @brief Builds a static tile-layer and tile-object preview image for a Tiled map.
class MapPreviewBuilder final {
public:
    [[nodiscard]] MapPreviewBuildResult buildPreview(std::string_view map_name, std::string_view tmj_path);
    void clearCache();

private:
    std::unordered_map<std::string, MapPreviewBuildResult> preview_cache_{};
    std::deque<std::string> preview_cache_order_{};

    void enforceCacheCapacity();
};

} // namespace game::ui
