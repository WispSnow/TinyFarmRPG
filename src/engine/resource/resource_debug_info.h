#pragma once

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
#include "engine/platform/gl_platform.h"
#include <entt/core/fwd.hpp>

namespace engine::resource {

struct TextureDebugInfo {
    entt::id_type id{};
    GLuint texture{0};
    int width{0};
    int height{0};
    std::string source;
    std::size_t memory_bytes{0};
};

struct FontAtlasPageDebugInfo {
    GLuint texture{0};
    int width{0};
    int height{0};
    int cursor_x{0};
    int cursor_y{0};
    int current_row_height{0};
};

struct FontDebugInfo {
    entt::id_type id{};
    int pixel_size{0};
    std::size_t glyph_count{0};
    float ascender{0.0f};
    float descender{0.0f};
    float line_height{0.0f};
    std::string source;
    std::size_t memory_bytes{0};
    std::vector<FontAtlasPageDebugInfo> atlas_pages;
};

enum class AudioKind {
    Sound,
    Music
};

struct AudioDebugInfo {
    entt::id_type id{};
    AudioKind kind{AudioKind::Sound};
    std::string source;
    std::uint32_t channels{0};
    std::uint32_t sample_rate{0};
    std::uint64_t frame_count{0};
    std::size_t sample_count{0};
    std::size_t memory_bytes{0};
    double duration_seconds{0.0};
};

} // namespace engine::resource
