#include "engine/core/context.h"
#include "engine/core/game_app.h"
#include "engine/utils/events.h"
#include "rmlui_test_scene.h"

#include <SDL3/SDL_main.h>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

void initializeEnvironment() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void setupInitialScene(engine::core::Context& context) {
    auto scene = std::make_unique<tools::rmlui::RmlUiTestScene>("RmlUiTester", context);
    context.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
}

} // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    initializeEnvironment();
    spdlog::set_level(spdlog::level::info);

    engine::core::GameApp app;
    app.registerSceneSetup(setupInitialScene);
    app.run();
    return 0;
}
