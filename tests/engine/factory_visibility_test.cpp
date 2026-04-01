// NOLINTBEGIN
#include <gtest/gtest.h>

#include <cstddef>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "engine/core/context.h"
#include "engine/async/main_thread_command_queue.h"
#include "engine/render/opengl/sprite_batch.h"
#include "engine/resource/resource_manager.h"

namespace {
template <typename T, typename... Args>
concept HasCreate = requires(Args&&... args) {
    { T::create(std::forward<Args>(args)...) } -> std::same_as<std::unique_ptr<T>>;
};
} // namespace

TEST(FactoryVisibilityTest, EngineFactoryRequiredTypesFollowFactoryPattern) {
#ifdef TF_ENABLE_DEBUG_UI
    EXPECT_TRUE((HasCreate<engine::core::Context,
                           engine::core::CoreServices,
                           engine::core::RenderServices,
                           engine::core::ResourceServices,
                           engine::core::UiServices,
                           engine::audio::AudioPlayer&,
                           engine::spatial::SpatialIndexManager&,
                           engine::debug::DebugUIManager&>));
    EXPECT_FALSE((std::is_constructible_v<engine::core::Context,
                                         engine::core::CoreServices,
                                         engine::core::RenderServices,
                                         engine::core::ResourceServices,
                                         engine::core::UiServices,
                                         engine::audio::AudioPlayer&,
                                         engine::spatial::SpatialIndexManager&,
                                         engine::debug::DebugUIManager&>));
#else
    EXPECT_TRUE((HasCreate<engine::core::Context,
                           engine::core::CoreServices,
                           engine::core::RenderServices,
                           engine::core::ResourceServices,
                           engine::core::UiServices,
                           engine::audio::AudioPlayer&,
                           engine::spatial::SpatialIndexManager&>));
    EXPECT_FALSE((std::is_constructible_v<engine::core::Context,
                                         engine::core::CoreServices,
                                         engine::core::RenderServices,
                                         engine::core::ResourceServices,
                                         engine::core::UiServices,
                                         engine::audio::AudioPlayer&,
                                         engine::spatial::SpatialIndexManager&>));
#endif
}

TEST(FactoryVisibilityTest, EngineResourceManagerUsesFactoryPattern) {
    EXPECT_TRUE((HasCreate<engine::resource::ResourceManager, entt::dispatcher*>));
    EXPECT_FALSE((std::is_constructible_v<engine::resource::ResourceManager, entt::dispatcher*>));
}

TEST(FactoryVisibilityTest, EngineSpriteBatchUsesFactoryPattern) {
    EXPECT_TRUE((HasCreate<engine::render::opengl::SpriteBatch, std::size_t>));
    EXPECT_FALSE((std::is_constructible_v<engine::render::opengl::SpriteBatch>));
}
// NOLINTEND
