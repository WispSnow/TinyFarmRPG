#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

namespace engine::resource {

struct RasterizedGlyphBitmap {
    std::uint32_t codepoint{0};
    std::uint32_t glyph_index{0};
    glm::ivec2 size{0};
    glm::ivec2 bearing{0};
    float advance{0.0f};
    std::vector<std::uint8_t> alpha{};
};

struct FontPreprocessData {
    entt::id_type font_id{0};
    int pixel_size{0};
    std::string source_path{};
    std::vector<RasterizedGlyphBitmap> glyphs{};
};

} // namespace engine::resource
