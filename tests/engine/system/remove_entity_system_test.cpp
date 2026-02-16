#include <gtest/gtest.h>

#include "engine/component/tags.h"
#include "engine/system/remove_entity_system.h"

#include <entt/entity/registry.hpp>

namespace engine::system {

TEST(RemoveEntitySystemTest, RemovesAllTaggedEntitiesInSingleUpdate) {
    entt::registry registry;

    const entt::entity keep = registry.create();

    const entt::entity a = registry.create();
    const entt::entity b = registry.create();
    const entt::entity c = registry.create();
    registry.emplace<engine::component::NeedRemoveTag>(a);
    registry.emplace<engine::component::NeedRemoveTag>(b);
    registry.emplace<engine::component::NeedRemoveTag>(c);

    RemoveEntitySystem system;
    system.update(registry);

    EXPECT_TRUE(registry.valid(keep));
    EXPECT_FALSE(registry.valid(a));
    EXPECT_FALSE(registry.valid(b));
    EXPECT_FALSE(registry.valid(c));
    EXPECT_EQ(registry.view<entt::entity>().size(), 1u);
    EXPECT_TRUE(registry.view<engine::component::NeedRemoveTag>().empty());
}

} // namespace engine::system
