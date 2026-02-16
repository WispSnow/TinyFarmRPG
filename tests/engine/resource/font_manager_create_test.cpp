#include <gtest/gtest.h>

#include <memory>

#include "engine/resource/font_manager.h"

namespace engine::resource {

TEST(FontManagerCreateTest, CreateReturnsInstance) {
    auto manager = FontManager::create();
    EXPECT_NE(manager, nullptr);
}

} // namespace engine::resource

