#include "engine/core/context.h"
#include "engine/core/game_app.h"
#include "engine/utils/events.h"
#include "events_scene.h"

#include <SDL3/SDL_main.h>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace {

void setupInitialScene(engine::core::Context& context) {
    auto scene = std::make_unique<learn::rmlui::EventsScene>("Events", context);
    context.getDispatcher().trigger<engine::utils::PushSceneEvent>(
        engine::utils::PushSceneEvent{std::move(scene)});
}

} // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    spdlog::set_level(spdlog::level::info);

    engine::core::GameApp app;
    app.registerSceneSetup(setupInitialScene);
    app.run();
    return 0;
}
