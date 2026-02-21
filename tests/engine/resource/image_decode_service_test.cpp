// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/resource/image_decode_service.h"

#include <filesystem>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::resource {
namespace {

TEST(ImageDecodeServiceTest, DecodesTextureToRgbaPixels) {
    const std::filesystem::path image_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/textures/UI/circle.png").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(image_path)) << image_path;

    const auto decoded = ImageDecodeService::decodeRGBA(image_path.string());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->valid());
    EXPECT_EQ(decoded->channels, 4);
    EXPECT_EQ(decoded->pixels.size(),
              static_cast<std::size_t>(decoded->width) * static_cast<std::size_t>(decoded->height) * 4U);
}

TEST(ImageDecodeServiceTest, ReturnsNulloptForInvalidFilePath) {
    const auto decoded = ImageDecodeService::decodeRGBA("assets/does/not/exist.png");
    EXPECT_FALSE(decoded.has_value());
}

} // namespace
} // namespace engine::resource
// NOLINTEND
