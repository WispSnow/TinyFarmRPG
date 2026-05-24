#include <gtest/gtest.h>

#include "game/component/chest_component.h"
#include "game/component/map_component.h"
#include "game/component/npc_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/defs/commands.h"
#include "game/system/system_helpers.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

struct DialogueSubscriber {
    entt::registry& registry;
    int handled{0};

    void onCommand(const game::defs::InteractCommand& command) {
        if (command.target == entt::null || !registry.valid(command.target)) return;
        if (game::system::helpers::isScriptedInteraction(registry, command.target)) return;
        if (!registry.all_of<game::component::DialogueComponent>(command.target)) return;
        ++handled;
    }
};

struct ChestSubscriber {
    entt::registry& registry;
    int handled{0};

    void onCommand(const game::defs::InteractCommand& command) {
        if (command.target == entt::null || !registry.valid(command.target)) return;
        if (game::system::helpers::isScriptedInteraction(registry, command.target)) return;
        if (!registry.all_of<game::component::ChestComponent>(command.target)) return;
        ++handled;
    }
};

struct RestSubscriber {
    entt::registry& registry;
    int handled{0};

    void onCommand(const game::defs::InteractCommand& command) {
        if (command.target == entt::null || !registry.valid(command.target)) return;
        if (game::system::helpers::isScriptedInteraction(registry, command.target)) return;
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

TEST(InteractionCommandPipelineTest, ScriptedInteraction_TargetsAreLuaExclusive) {
    entt::registry registry;
    entt::dispatcher dispatcher;

    const entt::entity player = registry.create();

    const entt::entity npc = registry.create();
    registry.emplace<game::component::DialogueComponent>(npc);
    registry.emplace<game::component::ScriptedInteractionComponent>(npc);

    const entt::entity chest = registry.create();
    registry.emplace<game::component::ChestComponent>(chest);
    registry.emplace<game::component::ScriptedInteractionComponent>(chest);

    const entt::entity rest_area = registry.create();
    registry.emplace<game::component::RestArea>(rest_area);
    registry.emplace<game::component::ScriptedInteractionComponent>(rest_area);

    DialogueSubscriber dialogue{registry};
    ChestSubscriber chest_subscriber{registry};
    RestSubscriber rest{registry};

    dispatcher.sink<game::defs::InteractCommand>().connect<&DialogueSubscriber::onCommand>(&dialogue);
    dispatcher.sink<game::defs::InteractCommand>().connect<&ChestSubscriber::onCommand>(&chest_subscriber);
    dispatcher.sink<game::defs::InteractCommand>().connect<&RestSubscriber::onCommand>(&rest);

    dispatcher.trigger(game::defs::InteractCommand{player, npc});
    dispatcher.trigger(game::defs::InteractCommand{player, chest});
    dispatcher.trigger(game::defs::InteractCommand{player, rest_area});

    EXPECT_EQ(dialogue.handled, 0);
    EXPECT_EQ(chest_subscriber.handled, 0);
    EXPECT_EQ(rest.handled, 0);
}

TEST(InteractionCommandPipelineTest, InteractSubscribersCheckScriptedInteractionHelper) {
    const std::vector<std::string_view> source_files{
        "src/game/system/dialogue_system.cpp",
        "src/game/system/quest_interaction_system.cpp",
        "src/game/system/recruitment_interaction_system.cpp",
        "src/game/system/shop_interaction_system.cpp",
        "src/game/system/chest_system.cpp",
        "src/game/system/rest_system.cpp",
        "src/game/system/closet_interaction_system.cpp",
    };

    for (const std::string_view file : source_files) {
        const auto path = (std::filesystem::path{PROJECT_SOURCE_DIR} / file).lexically_normal();
        const std::string source = readTextFile(path);
        ASSERT_FALSE(source.empty()) << path;
        EXPECT_NE(source.find("isScriptedInteraction"), std::string::npos) << path;
    }
}

} // namespace game::system
