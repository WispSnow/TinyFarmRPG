#include "game_entry.h"

#include "engine/core/context.h"
#include "engine/core/game_app.h"
#include "engine/utils/events.h"
#include "game/scene/title_scene.h"

#include <memory>
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

void setupInitialScene(engine::core::Context& context) {
    auto title_scene = std::make_unique<game::scene::TitleScene>("TitleScene", context);
    context.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(title_scene)});
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
