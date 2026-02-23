#include <gtest/gtest.h>

#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/system/state_system.h"
#include "engine/system/deferred_commands.h"
#include "engine/system/task_event_buffer.h"

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

TEST(StateSystemTest, DeferredPathClearsDirtyTagAfterDrain) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity entity = registry.create();
    registry.emplace<game::component::StateComponent>(entity);
    registry.emplace<game::component::StateDirtyTag>(entity);

    StateSystem system(registry, dispatcher);
    engine::system::DeferredCommands deferred;
    engine::system::TaskEventBuffer task_events;
    system.update(deferred, task_events);

    EXPECT_TRUE(registry.any_of<game::component::StateDirtyTag>(entity));
    deferred.drain(registry);
    EXPECT_FALSE(registry.any_of<game::component::StateDirtyTag>(entity));
}

} // namespace game::system
