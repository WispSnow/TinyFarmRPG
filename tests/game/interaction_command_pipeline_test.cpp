#include <gtest/gtest.h>

#include "game/component/chest_component.h"
#include "game/component/map_component.h"
#include "game/component/npc_component.h"
#include "game/defs/commands.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace {

struct DialogueSubscriber {
    entt::registry& registry;
    int handled{0};

    void onCommand(const game::defs::InteractCommand& command) {
        if (command.target == entt::null || !registry.valid(command.target)) return;
        if (!registry.all_of<game::component::DialogueComponent>(command.target)) return;
        ++handled;
    }
};

struct ChestSubscriber {
    entt::registry& registry;
    int handled{0};

    void onCommand(const game::defs::InteractCommand& command) {
        if (command.target == entt::null || !registry.valid(command.target)) return;
        if (!registry.all_of<game::component::ChestComponent>(command.target)) return;
        ++handled;
    }
};

struct RestSubscriber {
    entt::registry& registry;
    int handled{0};

    void onCommand(const game::defs::InteractCommand& command) {
        if (command.target == entt::null || !registry.valid(command.target)) return;
        if (!registry.all_of<game::component::RestArea>(command.target)) return;
        ++handled;
    }
};

} // namespace

namespace game::system {

TEST(InteractionCommandPipelineTest, InteractCommand_FansOutToComponentDrivenSubscribers) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity player = registry.create();

    const entt::entity npc = registry.create();
    registry.emplace<game::component::DialogueComponent>(npc);

    const entt::entity chest = registry.create();
    registry.emplace<game::component::ChestComponent>(chest);

    const entt::entity rest_area = registry.create();
    registry.emplace<game::component::RestArea>(rest_area);

    DialogueSubscriber dialogue{registry};
    ChestSubscriber chest_subscriber{registry};
    RestSubscriber rest{registry};

    dispatcher.sink<game::defs::InteractCommand>().connect<&DialogueSubscriber::onCommand>(&dialogue);
    dispatcher.sink<game::defs::InteractCommand>().connect<&ChestSubscriber::onCommand>(&chest_subscriber);
    dispatcher.sink<game::defs::InteractCommand>().connect<&RestSubscriber::onCommand>(&rest);

    dispatcher.trigger(game::defs::InteractCommand{player, npc});
    dispatcher.trigger(game::defs::InteractCommand{player, chest});
    dispatcher.trigger(game::defs::InteractCommand{player, rest_area});
    dispatcher.trigger(game::defs::InteractCommand{player, entt::null});

    EXPECT_EQ(dialogue.handled, 1);
    EXPECT_EQ(chest_subscriber.handled, 1);
    EXPECT_EQ(rest.handled, 1);
}

} // namespace game::system
