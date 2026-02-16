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
#include "engine/render/opengl/sprite_batch.h"
#include "engine/resource/resource_manager.h"
#include "engine/ui/ui_button.h"

namespace {
template <typename T, typename... Args>
concept HasCreate = requires(Args&&... args) {
    { T::create(std::forward<Args>(args)...) } -> std::same_as<std::unique_ptr<T>>;
};
} // namespace

TEST(FactoryVisibilityTest, EngineFactoryRequiredTypesFollowFactoryPattern) {
#ifdef TF_ENABLE_DEBUG_UI
    EXPECT_TRUE((HasCreate<engine::core::Context,
                           entt::dispatcher&,
                           engine::input::InputManager&,
                           engine::render::opengl::GLRenderer&,
                           engine::render::Renderer&,
                           engine::render::Camera&,
                           engine::render::TextRenderer&,
                           engine::resource::ResourceManager&,
                           engine::audio::AudioPlayer&,
                           engine::core::GameState&,
                           engine::core::Time&,
                           engine::debug::DebugUIManager&,
                           engine::spatial::SpatialIndexManager&>));
    EXPECT_FALSE((std::is_constructible_v<engine::core::Context,
                                         entt::dispatcher&,
                                         engine::input::InputManager&,
                                         engine::render::opengl::GLRenderer&,
                                         engine::render::Renderer&,
                                         engine::render::Camera&,
                                         engine::render::TextRenderer&,
                                         engine::resource::ResourceManager&,
                                         engine::audio::AudioPlayer&,
                                         engine::core::GameState&,
                                         engine::core::Time&,
                                         engine::debug::DebugUIManager&,
                                         engine::spatial::SpatialIndexManager&>));
#else
    EXPECT_TRUE((HasCreate<engine::core::Context,
                           entt::dispatcher&,
                           engine::input::InputManager&,
                           engine::render::opengl::GLRenderer&,
                           engine::render::Renderer&,
                           engine::render::Camera&,
                           engine::render::TextRenderer&,
                           engine::resource::ResourceManager&,
                           engine::audio::AudioPlayer&,
                           engine::core::GameState&,
                           engine::core::Time&,
                           engine::spatial::SpatialIndexManager&>));
    EXPECT_FALSE((std::is_constructible_v<engine::core::Context,
                                         entt::dispatcher&,
                                         engine::input::InputManager&,
                                         engine::render::opengl::GLRenderer&,
                                         engine::render::Renderer&,
                                         engine::render::Camera&,
                                         engine::render::TextRenderer&,
                                         engine::resource::ResourceManager&,
                                         engine::audio::AudioPlayer&,
                                         engine::core::GameState&,
                                         engine::core::Time&,
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

TEST(FactoryVisibilityTest, EngineUiFactoryRequiredTypesFollowFactoryPattern) {
    EXPECT_TRUE((HasCreate<engine::ui::UIButton, engine::core::Context&, std::string_view>));
    EXPECT_FALSE((std::is_constructible_v<engine::ui::UIButton,
                                         engine::core::Context&,
                                         glm::vec2,
                                         glm::vec2,
                                         std::function<void()>,
                                         std::function<void()>,
                                         std::function<void()>>));
}
// NOLINTEND
