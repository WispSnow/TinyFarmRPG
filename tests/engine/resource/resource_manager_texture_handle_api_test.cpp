#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include "engine/resource/resource_manager.h"

namespace engine::resource {

TEST(ResourceManagerTextureHandleApiTest, TextureApisReturnTextureHandle) {
    using LoadByIdReturn = decltype(std::declval<ResourceManager&>().loadTexture(entt::id_type{}, std::string_view{}));
    using LoadByHashReturn = decltype(std::declval<ResourceManager&>().loadTexture(std::declval<entt::hashed_string>()));
    using GetByIdReturn = decltype(std::declval<ResourceManager&>().getTexture(entt::id_type{}, std::string_view{}));
    using GetByHashReturn = decltype(std::declval<ResourceManager&>().getTexture(std::declval<entt::hashed_string>()));

    static_assert(std::is_same_v<LoadByIdReturn, TextureHandle>);
    static_assert(std::is_same_v<LoadByHashReturn, TextureHandle>);
    static_assert(std::is_same_v<GetByIdReturn, TextureHandle>);
    static_assert(std::is_same_v<GetByHashReturn, TextureHandle>);
    SUCCEED();
}

TEST(ResourceManagerTextureHandleApiTest, FailedTextureLoadDoesNotLeaveDebugEntry) {
    entt::dispatcher dispatcher;
    auto resource_manager = ResourceManager::create(&dispatcher);
    ASSERT_NE(resource_manager, nullptr);

    constexpr std::string_view kMissingTexturePath = "tests/data/texture_missing_for_handle_api_test.png";
    constexpr entt::id_type texture_id = entt::hashed_string{kMissingTexturePath.data(), kMissingTexturePath.size()};

    const TextureHandle loaded = resource_manager->loadTexture(texture_id, kMissingTexturePath);
    EXPECT_FALSE(loaded);

    const TextureHandle fetched = resource_manager->getTexture(texture_id);
    EXPECT_FALSE(fetched);

    EXPECT_EQ(resource_manager->getTextureSize(texture_id), glm::vec2(0.0f, 0.0f));

    const auto debug_infos = resource_manager->getTextureDebugInfo();
    const auto it = std::find_if(debug_infos.begin(), debug_infos.end(), [](const TextureDebugInfo& info) {
        return info.id == texture_id;
    });
    EXPECT_EQ(it, debug_infos.end());
}

} // namespace engine::resource
