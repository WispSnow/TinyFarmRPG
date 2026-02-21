#include "engine/resource/font_preprocess_service.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

namespace engine::resource {

std::optional<FontPreprocessData> FontPreprocessService::rasterizeGlyphs(entt::id_type font_id,
                                                                          int pixel_size,
                                                                          std::string_view file_path,
                                                                          std::u32string_view codepoints) {
    if (font_id == 0 || pixel_size <= 0 || file_path.empty() || codepoints.empty()) {
        return std::nullopt;
    }

    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0 || !library) {
        return std::nullopt;
    }

    FT_Face face = nullptr;
    const std::string file_path_string(file_path);
    if (FT_New_Face(library, file_path_string.c_str(), 0, &face) != 0 || !face) {
        FT_Done_FreeType(library);
        return std::nullopt;
    }

    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size)) != 0) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return std::nullopt;
    }

    FontPreprocessData preprocessed{};
    preprocessed.font_id = font_id;
    preprocessed.pixel_size = pixel_size;
    preprocessed.source_path = file_path_string;
    preprocessed.glyphs.reserve(codepoints.size());

    for (const char32_t codepoint : codepoints) {
        const FT_UInt glyph_index = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
        if (glyph_index == 0) {
            continue;
        }

        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) != 0) {
            continue;
        }
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            continue;
        }

        const FT_Bitmap& bitmap = face->glyph->bitmap;
        const int width = static_cast<int>(bitmap.width);
        const int height = static_cast<int>(bitmap.rows);
        if (width <= 0 || height <= 0 || !bitmap.buffer) {
            continue;
        }

        const int source_pitch = std::abs(bitmap.pitch);
        RasterizedGlyphBitmap glyph{};
        glyph.codepoint = static_cast<std::uint32_t>(codepoint);
        glyph.glyph_index = static_cast<std::uint32_t>(glyph_index);
        glyph.size = {width, height};
        glyph.bearing = {face->glyph->bitmap_left, face->glyph->bitmap_top};
        glyph.advance = static_cast<float>(face->glyph->advance.x) / 64.0f;
        glyph.alpha.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

        if (source_pitch == width) {
            std::memcpy(glyph.alpha.data(), bitmap.buffer, glyph.alpha.size());
        } else {
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src = bitmap.buffer + static_cast<std::size_t>(y) * static_cast<std::size_t>(source_pitch);
                std::uint8_t* dst = glyph.alpha.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
                std::memcpy(dst, src, static_cast<std::size_t>(width));
            }
        }

        preprocessed.glyphs.push_back(std::move(glyph));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    if (preprocessed.glyphs.empty()) {
        return std::nullopt;
    }
    return preprocessed;
}

} // namespace engine::resource
