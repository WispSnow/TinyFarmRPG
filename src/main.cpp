#define SDL_MAIN_USE_CALLBACKS 1

#include "game/game_entry.h"

#include "engine/core/game_app.h"

#include <SDL3/SDL_main.h>
#include <cstddef>
#include <memory>
#include <spdlog/spdlog.h>

#if defined(__EMSCRIPTEN__) && defined(TF_ENABLE_DEBUG_UI)
#include <emscripten.h>
#endif

namespace {

struct AppState {
    std::unique_ptr<engine::core::GameApp> app;
};

#if defined(__EMSCRIPTEN__) && defined(TF_ENABLE_DEBUG_UI)
AppState* g_active_app_state{nullptr};
#endif

[[nodiscard]] AppState* toAppState(void* appstate) {
    return static_cast<AppState*>(appstate);
}

#if defined(__EMSCRIPTEN__) && defined(TF_ENABLE_DEBUG_UI)
[[nodiscard]] engine::core::GameApp* activeApp() {
    return g_active_app_state != nullptr ? g_active_app_state->app.get() : nullptr;
}
#endif

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

#if defined(__EMSCRIPTEN__) && defined(TF_ENABLE_DEBUG_UI)
    g_active_app_state = state.get();
#endif
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
#if defined(__EMSCRIPTEN__) && defined(TF_ENABLE_DEBUG_UI)
    if (g_active_app_state == state.get()) {
        g_active_app_state = nullptr;
    }
#endif
    if (state && state->app) {
        state->app->shutdown();
    }
}

#if defined(__EMSCRIPTEN__) && defined(TF_ENABLE_DEBUG_UI)
extern "C" EMSCRIPTEN_KEEPALIVE int tf_web_debug_ui_available() {
    auto* app = activeApp();
    return app != nullptr && app->isDebugUIAvailable() ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int tf_web_debug_toggle_panel(int category) {
    auto* app = activeApp();
    if (app == nullptr || category < 0) {
        return 0;
    }
    return app->toggleDebugPanel(static_cast<std::size_t>(category)) ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int tf_web_debug_set_panel_visible(int category, int visible) {
    auto* app = activeApp();
    if (app == nullptr || category < 0) {
        return 0;
    }
    return app->setDebugPanelVisible(static_cast<std::size_t>(category), visible != 0) ? 1 : 0;
}
#endif
