#include "game_entry.h"

#include "engine/core/context.h"
#include "engine/core/game_app.h"
#include "engine/utils/events.h"
#ifdef TF_WEB_DIRECT_MAP_BOOT
#include "game/scene/game_scene.h"
#else
#include "game/scene/title_scene.h"
#endif

#include <memory>
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

void setupInitialScene(engine::core::Context& context) {
#ifdef TF_WEB_DIRECT_MAP_BOOT
    spdlog::info("Web direct map boot: pushing GameScene at home_exterior.");
    auto game_scene = std::make_unique<game::scene::GameScene>(
        "GameScene",
        context,
        nullptr,
        game::scene::NewGameOptions{});
    context.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(game_scene)});
#else
    auto title_scene = std::make_unique<game::scene::TitleScene>("TitleScene", context);
    context.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(title_scene)});
#endif
}

} // namespace

namespace game {

void initializeEnvironment() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::unique_ptr<engine::core::GameApp> createApp() {
    auto app = std::make_unique<engine::core::GameApp>();
    app->registerSceneSetup(setupInitialScene);
    return app;
}

int run() {
    initializeEnvironment();
    spdlog::set_level(spdlog::level::info);

    auto app = createApp();
    app->run();
    return 0;
}

} // namespace game
