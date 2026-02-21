// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/resource/font_preprocess_service.h"

#include <filesystem>
#include <string>

#include <entt/core/hashed_string.hpp>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::resource {
namespace {

TEST(FontPreprocessServiceTest, RasterizesGlyphBitmapsOnCpu) {
    const std::filesystem::path font_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/fonts/VonwaonBitmap-16px.ttf").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(font_path)) << font_path;

    const entt::id_type font_id = entt::hashed_string{"engine/font/ui_default"}.value();
    const auto preprocessed = FontPreprocessService::rasterizeGlyphs(font_id, 16, font_path.string(), U"Farm123");
    ASSERT_TRUE(preprocessed.has_value());
    EXPECT_EQ(preprocessed->font_id, font_id);
    EXPECT_EQ(preprocessed->pixel_size, 16);
    EXPECT_FALSE(preprocessed->glyphs.empty());

    for (const auto& glyph : preprocessed->glyphs) {
        EXPECT_GT(glyph.size.x, 0);
        EXPECT_GT(glyph.size.y, 0);
        EXPECT_FALSE(glyph.alpha.empty());
    }
}

} // namespace
} // namespace engine::resource
// NOLINTEND
