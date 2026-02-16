#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <string_view>
#include <memory>
#include <imgui.h>

namespace engine::render::opengl {

/**
 * @brief ImGui SDL/OpenGL 后端的包装器。
 *
 * 将 ImGui 的初始化与配置封装在渲染器内部，使引擎其他部分仅需调用 begin/end 风格的接口即可。
 * 当禁用 ImGui 时，游戏逻辑可以直接跳过 begin/end 调用，无需引入任何 ImGui 相关头文件。
 */
class ImGuiLayer final {

    SDL_Window* window_{nullptr};
    SDL_GLContext context_{nullptr};
    std::string glsl_version_{};

public:
    [[nodiscard]] static std::unique_ptr<ImGuiLayer> create(SDL_Window* window,
                                                            SDL_GLContext context,
                                                            std::string_view glsl_version = "#version 330");
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    void clean();
    void newFrame();
    void endFrame();

    /**
     * @brief 将 SDL 事件转发给 ImGui。
     * @param event SDL 事件。
     */
    void processEvent(const SDL_Event& event);

    /**
     * @brief 便捷的 ImGuiIO 访问器。如果未初始化，则返回未定义。
     * @return ImGuiIO 引用。
     */
    ImGuiIO& io();

private:
    ImGuiLayer() = default;

    /**
     * @brief 使用 SDL3/OpenGL 后端初始化 ImGui。
     * @param window 有效的 SDL 窗口句柄。
     * @param context 绑定到窗口的 OpenGL 上下文。
     * @param glsl_version 传递给 OpenGL 后端的 GLSL 版本字符串。
     * @return 成功返回 true，否则返回 false。
     */
    [[nodiscard]] bool init(SDL_Window* window, SDL_GLContext context, std::string_view glsl_version = "#version 330");

    void configureStyle();
    void configureFonts();
};

} // namespace engine::render::opengl
