#define SDL_MAIN_USE_CALLBACKS 1

#include "game/game_entry.h"

#include "engine/core/game_app.h"

#include <SDL3/SDL_main.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace {

struct AppState {
    std::unique_ptr<engine::core::GameApp> app;
};

[[nodiscard]] AppState* toAppState(void* appstate) {
    return static_cast<AppState*>(appstate);
}

} // namespace

extern "C" SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int /*argc*/, char* /*argv*/[]) {
    game::initializeEnvironment();
    spdlog::set_level(spdlog::level::info);

    auto state = std::make_unique<AppState>();
    state->app = game::createApp();
    if (!state->app || !state->app->init()) {
        spdlog::error("TinyFarmRPG: SDL_AppInit 初始化失败。");
        if (state->app) {
            state->app->shutdown();
        }
        return SDL_APP_FAILURE;
    }

    *appstate = state.release();
    return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = toAppState(appstate);
    if (state == nullptr || state->app == nullptr || event == nullptr) {
        return SDL_APP_FAILURE;
    }

    state->app->handleSdlEvent(*event);
    return state->app->isRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

extern "C" SDL_AppResult SDLCALL SDL_AppIterate(void* appstate) {
    auto* state = toAppState(appstate);
    if (state == nullptr || state->app == nullptr) {
        return SDL_APP_FAILURE;
    }

    return state->app->tickFrame(engine::core::GameApp::EventPumpMode::ExternalCallbacks)
        ? SDL_APP_CONTINUE
        : SDL_APP_SUCCESS;
}

extern "C" void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) {
    std::unique_ptr<AppState> state{toAppState(appstate)};
    if (state && state->app) {
        state->app->shutdown();
    }
}
