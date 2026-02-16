#pragma once

#include <SDL3/SDL.h>
#include <limits>

namespace engine::render::opengl {  

/// @brief OpenGL 渲染初始化参数结构体
struct RendererInitParams {
    // 特殊哨兵值，表示“保持交换帧间隔不变”。SDL 解释 INT_MIN 为“不关心”。
    static constexpr int SWAP_INTERVAL_DONT_CARE{std::numeric_limits<int>::min()};

    // 请求的 OpenGL 主/次版本号。默认使用 3.3 Core Profile。
    int gl_major_version_{3};
    int gl_minor_version_{3};

    // 上下文配置掩码（核心/兼容性/ES）。SDL_GL_CONTEXT_PROFILE_CORE 排除了已弃用的固定功能 API。
    SDL_GLProfile profile_mask_{SDL_GL_CONTEXT_PROFILE_CORE};

    // 额外的 SDL_GL 上下文标志（如 SDL_GL_CONTEXT_DEBUG_FLAG）。
    SDL_GLContextFlag context_flags_{0};

    // 是否请求双缓冲的默认帧缓冲区。
    bool double_buffer_{true};

    // 默认帧缓冲的深度/模板缓冲精度。通常请求 24 位深度和 8 位模板
    int depth_bits_{24};
    int stencil_bits_{8};

    // 多重采样配置（MSAA 缓冲区/采样数量）。默认禁用，调用方可启用以获得全场景 MSAA。
    int multi_sample_buffers_{0};
    int multi_sample_samples_{0};

    // 是否请求 SDL 提供支持 sRGB 的帧缓冲区，便于最终混合时进行伽玛校正。
    int framebuffer_SRGB_capable_{1};

    // 交换间隔（即垂直同步 VSync）。1 = 启用 vsync；0 = 立即交换等。用 SWAP_INTERVAL_DONT_CARE 表示不更改。
    int swap_interval_{1};
};

} // namespace engine::render::opengl