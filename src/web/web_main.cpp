#include "engine/platform/gl_platform.h"

#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

#include <array>
#include <cstdio>

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;

constexpr const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec3 a_color;

uniform float u_time_seconds;

out vec3 v_color;

void main() {
    float pulse = sin(u_time_seconds * 1.7) * 0.035;
    vec2 position = a_position * (0.68 + pulse);
    gl_Position = vec4(position, 0.0, 1.0);
    v_color = a_color;
}
)";

constexpr const char* kFragmentShader = R"(#version 300 es
precision mediump float;

in vec3 v_color;
out vec4 frag_color;

void main() {
    frag_color = vec4(v_color, 1.0);
}
)";

struct WebApp {
    SDL_Window* window{};
    SDL_GLContext gl_context{};
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLint time_uniform{-1};
    Uint64 start_ticks{};
    bool running{true};
};

void logGlInfo() {
    const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::printf("TinyFarmRPG WebGL vendor: %s\n", vendor ? vendor : "(unknown)");
    std::printf("TinyFarmRPG WebGL renderer: %s\n", renderer ? renderer : "(unknown)");
    std::printf("TinyFarmRPG GL version: %s\n", version ? version : "(unknown)");
}

void logPreloadedFile(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        std::printf("preload missing: %s\n", path);
        return;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fclose(file);
    std::printf("preload ok: %s (%ld bytes)\n", path, size);
}

void logPreloadSmoke() {
    logPreloadedFile("/assets/data/resource_mapping.json");
    logPreloadedFile("/config/window.json");
    logPreloadedFile("/ui/rmlui/scenes/title.rml");
    logPreloadedFile("/assets/maps/home_exterior.tmj");
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    std::array<char, 1024> log{};
    GLsizei length = 0;
    glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &length, log.data());
    std::fprintf(stderr, "Shader compile failed: %.*s\n", length, log.data());
    glDeleteShader(shader);
    return 0;
}

GLuint createProgram() {
    const GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, kVertexShader);
    if (vertex_shader == 0) {
        return 0;
    }

    const GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    std::array<char, 1024> log{};
    GLsizei length = 0;
    glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &length, log.data());
    std::fprintf(stderr, "Program link failed: %.*s\n", length, log.data());
    glDeleteProgram(program);
    return 0;
}

bool createQuad(WebApp& app) {
    const std::array<float, 30> vertices{
        -0.72F, -0.44F, 0.26F, 0.74F, 0.54F,
        0.72F, -0.44F, 0.91F, 0.77F, 0.36F,
        -0.72F, 0.44F, 0.47F, 0.58F, 0.94F,
        -0.72F, 0.44F, 0.47F, 0.58F, 0.94F,
        0.72F, -0.44F, 0.91F, 0.77F, 0.36F,
        0.72F, 0.44F, 0.98F, 0.42F, 0.60F,
    };

    glGenVertexArrays(1, &app.vao);
    glBindVertexArray(app.vao);

    glGenBuffers(1, &app.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, app.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = 5 * static_cast<GLsizei>(sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return glGetError() == GL_NO_ERROR;
}

void shutdown(WebApp& app) {
    if (app.vbo != 0) {
        glDeleteBuffers(1, &app.vbo);
        app.vbo = 0;
    }
    if (app.vao != 0) {
        glDeleteVertexArrays(1, &app.vao);
        app.vao = 0;
    }
    if (app.program != 0) {
        glDeleteProgram(app.program);
        app.program = 0;
    }
    if (app.gl_context != nullptr) {
        SDL_GL_DestroyContext(app.gl_context);
        app.gl_context = nullptr;
    }
    if (app.window != nullptr) {
        SDL_DestroyWindow(app.window);
        app.window = nullptr;
    }
    SDL_Quit();
}

void frame(void* user_data) {
    auto* app = static_cast<WebApp*>(user_data);
    if (app == nullptr || !app->running) {
        emscripten_cancel_main_loop();
        return;
    }

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            app->running = false;
            emscripten_cancel_main_loop();
            shutdown(*app);
            return;
        }
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.10F, 0.14F, 0.19F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    const float elapsed_seconds =
        static_cast<float>(SDL_GetTicks() - app->start_ticks) / 1000.0F;
    glUseProgram(app->program);
    glUniform1f(app->time_uniform, elapsed_seconds);
    glBindVertexArray(app->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);

    SDL_GL_SwapWindow(app->window);
}

bool initialize(WebApp& app) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    app.window = SDL_CreateWindow(
        "TinyFarmRPG Web Walking Skeleton",
        kWindowWidth,
        kWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (app.window == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        shutdown(app);
        return false;
    }

    app.gl_context = SDL_GL_CreateContext(app.window);
    if (app.gl_context == nullptr) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        shutdown(app);
        return false;
    }

    SDL_GL_MakeCurrent(app.window, app.gl_context);
    SDL_GL_SetSwapInterval(1);
    logGlInfo();
    logPreloadSmoke();

    app.program = createProgram();
    if (app.program == 0) {
        shutdown(app);
        return false;
    }
    app.time_uniform = glGetUniformLocation(app.program, "u_time_seconds");

    if (!createQuad(app)) {
        std::fprintf(stderr, "Failed to create WebGL quad resources.\n");
        shutdown(app);
        return false;
    }

    app.start_ticks = SDL_GetTicks();
    return true;
}

} // namespace

int main() {
    static WebApp app{};
    if (!initialize(app)) {
        return 1;
    }

    emscripten_set_main_loop_arg(frame, &app, 0, true);
    return 0;
}
