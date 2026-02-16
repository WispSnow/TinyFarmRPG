#include <gtest/gtest.h>

#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/system/state_system.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::system {

TEST(StateSystemTest, ClearsAllStateDirtyTagsInSingleUpdate) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity a = registry.create();
    const entt::entity b = registry.create();
    const entt::entity c = registry.create();

    registry.emplace<game::component::StateComponent>(a);
    registry.emplace<game::component::StateComponent>(b);
    registry.emplace<game::component::StateComponent>(c);

    registry.emplace<game::component::StateDirtyTag>(a);
    registry.emplace<game::component::StateDirtyTag>(b);
    registry.emplace<game::component::StateDirtyTag>(c);

    StateSystem system(registry, dispatcher);
    system.update();

    EXPECT_FALSE(registry.any_of<game::component::StateDirtyTag>(a));
    EXPECT_FALSE(registry.any_of<game::component::StateDirtyTag>(b));
    EXPECT_FALSE(registry.any_of<game::component::StateDirtyTag>(c));
}

} // namespace game::system

