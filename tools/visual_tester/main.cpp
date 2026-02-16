#include "engine/core/game_app.h"
#include "engine/core/context.h"
#include "engine/utils/events.h"
#include "visual_test_suite_scene.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_main.h>
#include <entt/signal/dispatcher.hpp>

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
    auto visual_tests = std::make_unique<tools::visual::VisualTestSuiteScene>("VisualTests", context);
    context.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(visual_tests)});
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

