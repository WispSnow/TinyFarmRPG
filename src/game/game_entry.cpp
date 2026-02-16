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

void initialize_environment() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void setupInitialScene(engine::core::Context& context) {
    auto title_scene = std::make_unique<game::scene::TitleScene>("TitleScene", context);
    context.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(title_scene)});
}

} // namespace

namespace game {

int run() {
    initialize_environment();
    spdlog::set_level(spdlog::level::info);

    engine::core::GameApp app;
    app.registerSceneSetup(setupInitialScene);
    app.run();
    return 0;
}

} // namespace game
