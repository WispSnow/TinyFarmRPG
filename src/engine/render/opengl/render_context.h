#pragma once

/**
 * @file render_context.h
 * @brief 封装 SDL 的 OpenGL 属性配置、上下文创建及销毁的管理类。
 *
 * 该文件定义了 RenderContext 类，专门用于配置 SDL 的 OpenGL 属性、创建 OpenGL 上下文以及资源的安全销毁。
 * 通过将 SDL/OpenGL 的设置与生命周期管理集中在此类中，GLRenderer 可以专注于渲染逻辑，
 * 而具体的 SDL/OpenGL 实现细节得到更好的隔离和封装。
 */

#include "renderer_init_params.h"
#include <string_view>
#include <memory>
#include <SDL3/SDL.h>
#include <nlohmann/json_fwd.hpp>

namespace engine::render::opengl {

class RenderContext final {
    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    RendererInitParams params_{};

public:
    /**
     * @brief 创建 RenderContext 实例。
    * @param window SDL_Window* 窗口指针。
     * @param params_json_path std::string_view 配置文件路径。
     * @return 创建的 RenderContext 实例, 失败返回 nullptr。
    */
    [[nodiscard]] static std::unique_ptr<RenderContext> create(SDL_Window* window, std::string_view params_json_path = "");
    
    ~RenderContext();

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;
    RenderContext(RenderContext&&) = delete;
    RenderContext& operator=(RenderContext&&) = delete;

    void clean();

    SDL_Window* window() const { return window_; }
    SDL_GLContext context() const { return context_; }

    [[nodiscard]] bool setSwapInterval(int interval);
    [[nodiscard]] int getSwapInterval() const;

    void swapWindow();
    const RendererInitParams& params() const { return params_; }

private:
    RenderContext() = default;

    /**
     * @brief 初始化渲染上下文。
     * @param window SDL_Window* 窗口指针。
     * @param params_json_path std::string_view 配置文件路径。
     * @return 是否成功。
     */
    [[nodiscard]] bool init(SDL_Window* window, std::string_view params_json_path = "");

    [[nodiscard]] bool setAttributes();
    [[nodiscard]] bool createContext();

    bool loadFromFile(std::string_view params_json_path);
    void fromJson(const nlohmann::json& j);
};
} // namespace engine::render::opengl
