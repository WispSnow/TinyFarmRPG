#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>
#include <GLES3/gl3.h>

#include <cstdio>

namespace {

struct SmokeApp {
    SDL_Window* window{};
    SDL_GLContext gl_context{};
    bool running{true};
};

void renderFrame(void* user_data) {
    auto* app = static_cast<SmokeApp*>(user_data);
    if (app == nullptr || !app->running) {
        emscripten_cancel_main_loop();
        return;
    }

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            app->running = false;
            emscripten_cancel_main_loop();
            return;
        }
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.10F, 0.16F, 0.22F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(app->window);
}

} // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    auto* app = new SmokeApp{};
    app->window = SDL_CreateWindow(
        "TinyFarmRPG SDL3 WebGL2 Smoke",
        960,
        540,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (app->window == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        delete app;
        return 1;
    }

    app->gl_context = SDL_GL_CreateContext(app->window);
    if (app->gl_context == nullptr) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        delete app;
        return 1;
    }

    SDL_GL_MakeCurrent(app->window, app->gl_context);
    SDL_GL_SetSwapInterval(1);
    emscripten_set_main_loop_arg(renderFrame, app, 0, true);
    return 0;
}
