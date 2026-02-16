#include "imgui_layer.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

namespace engine::render::opengl {

std::unique_ptr<ImGuiLayer> ImGuiLayer::create(SDL_Window* window, SDL_GLContext context, std::string_view glsl_version) {
    auto layer = std::unique_ptr<ImGuiLayer>(new ImGuiLayer());
    if (!layer->init(window, context, glsl_version)) {
        return nullptr;
    }
    return layer;
}

ImGuiLayer::~ImGuiLayer() {
    clean();
}

bool ImGuiLayer::init(SDL_Window* window, SDL_GLContext context, std::string_view glsl_version) {
    if (window == nullptr || context == nullptr) {
        spdlog::error("ImGuiLayer::init 收到无效的 SDL_Window 或 SDL_GLContext。");
        return false;
    }

    window_ = window;
    context_ = context;
    glsl_version_ = glsl_version.empty() ? "#version 330" : std::string(glsl_version);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    configureStyle();
    configureFonts();

    // 初始化 SDL3/OpenGL 后端
    if (!ImGui_ImplSDL3_InitForOpenGL(window_, context_)) {
        spdlog::error("ImGui_ImplSDL3_InitForOpenGL 初始化失败。");
        clean();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version_.c_str())) {
        spdlog::error("ImGui_ImplOpenGL3_Init 初始化失败。");
        clean();
        return false;
    }

    spdlog::trace("ImGuiLayer 初始化成功。");
    return true;
}

void ImGuiLayer::clean() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (!context) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData) {
        ImGui_ImplOpenGL3_Shutdown();
    } else {
        spdlog::warn("ImGuiLayer::shutdown: OpenGL 后端未初始化或已关闭。");
    }
    if (io.BackendPlatformUserData) {
        ImGui_ImplSDL3_Shutdown();
    } else {
        spdlog::warn("ImGuiLayer::shutdown: SDL 平台后端未初始化或已关闭。");
    }
    ImGui::DestroyContext();

    window_ = nullptr;
    context_ = nullptr;
    glsl_version_.clear();
    spdlog::info("ImGuiLayer 已清理。");
}

void ImGuiLayer::newFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::processEvent(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(&event);
}

ImGuiIO& ImGuiLayer::io() {
    return ImGui::GetIO();
}

void ImGuiLayer::configureStyle() {
    ImGui::StyleColorsDark();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    constexpr float MAIN_SCALE = 1.0f;
    auto& style = ImGui::GetStyle();
    style.ScaleAllSizes(MAIN_SCALE);
    style.FontScaleDpi = MAIN_SCALE;

    constexpr float WINDOW_ALPHA = 0.5f;
    style.Colors[ImGuiCol_WindowBg].w = WINDOW_ALPHA;
    style.Colors[ImGuiCol_PopupBg].w = WINDOW_ALPHA;
}

void ImGuiLayer::configureFonts() {
    auto& io = ImGui::GetIO();
    const char* font_path = "assets/fonts/VonwaonBitmap-16px.ttf";
    constexpr float FONT_SIZE = 16.0f;
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        font_path,
        FONT_SIZE,
        nullptr,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon()
    );
    if (!font) {
        io.Fonts->AddFontDefault();
        spdlog::warn("ImGuiLayer: 无法加载中文字体 '{}'", font_path);
    }
}

} // namespace engine::render::opengl
