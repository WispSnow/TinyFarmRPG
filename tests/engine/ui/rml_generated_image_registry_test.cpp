#include <gtest/gtest.h>

#include "engine/resource/decoded_image.h"
#include "engine/ui/rmlui/rml_generated_image_registry.h"

#include <utility>

namespace engine::ui::rmlui {
namespace {

[[nodiscard]] engine::resource::DecodedImage makeImage() {
    engine::resource::DecodedImage image{};
    image.width = 1;
    image.height = 1;
    image.channels = 4;
    image.pixels = {255, 0, 0, 255};
    return image;
}

TEST(RmlGeneratedImageRegistryTest, RegisterFindAndUnregisterImage) {
    RmlGeneratedImageRegistry registry;
    auto registration = registry.registerImage("generated://test/image/1", makeImage());

    ASSERT_TRUE(registration.valid());
    const auto* found = registry.find("generated://test/image/1");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->width, 1);
    EXPECT_EQ(found->height, 1);

    registration.reset();
    EXPECT_EQ(registry.find("generated://test/image/1"), nullptr);
}

TEST(RmlGeneratedImageRegistryTest, OptionalTextureFilterOverrideFollowsRegistrationLifetime) {
    RmlGeneratedImageRegistry registry;
    auto registration = registry.registerImage(
        "generated://test/image/linear",
        makeImage(),
        RmlUiTextureFilterMode::Linear);

    const auto filter = registry.textureFilterOverrideFor("generated://test/image/linear");
    ASSERT_TRUE(filter.has_value());
    EXPECT_EQ(*filter, RmlUiTextureFilterMode::Linear);

    registration.reset();
    EXPECT_FALSE(registry.textureFilterOverrideFor("generated://test/image/linear").has_value());
}

TEST(RmlGeneratedImageRegistryTest, RegistrationReleasesOnMoveAssignment) {
    RmlGeneratedImageRegistry registry;
    auto first = registry.registerImage("generated://test/image/1", makeImage());
    auto second = registry.registerImage("generated://test/image/2", makeImage());

    first = std::move(second);

    EXPECT_EQ(registry.find("generated://test/image/1"), nullptr);
    EXPECT_NE(registry.find("generated://test/image/2"), nullptr);
    EXPECT_TRUE(first.valid());
}

TEST(RmlGeneratedImageRegistryTest, DetectsGeneratedSourceScheme) {
    EXPECT_TRUE(RmlGeneratedImageRegistry::isGeneratedSource("generated://map-preview/home/1"));
    EXPECT_FALSE(RmlGeneratedImageRegistry::isGeneratedSource("assets/maps/home_exterior.png"));
}

} // namespace
} // namespace engine::ui::rmlui
