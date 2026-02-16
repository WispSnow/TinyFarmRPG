/**
 * @file render_context.cpp
 * @brief SDL/OpenGL 渲染上下文管理实现文件
 *
 * 本文件封装了 SDL 和 OpenGL 的初始化与管理流程，确保渲染器始终拥有有效的 OpenGL 上下文。
 * 
 * 主要职责包括：
 * - 配置 OpenGL 属性
 * - 创建并管理 OpenGL 上下文
 * - 加载 GLAD，用于解析 OpenGL 函数指针
 * - 配置帧交换间隔（如 VSync）
 * - 实现上下文的完整和安全销毁逻辑
 */
#include "render_context.h"
#include <fstream>
#include <filesystem>
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace engine::render::opengl {

std::unique_ptr<RenderContext> RenderContext::create(SDL_Window* window, std::string_view params_json_path) {
    auto context = std::unique_ptr<RenderContext>(new RenderContext());
    if (!context->init(window, params_json_path)) {
        spdlog::error("创建 RenderContext 失败");
        return nullptr;
    }
    return context;
}

RenderContext::~RenderContext() {
    clean();
}

bool RenderContext::init(SDL_Window* window, std::string_view params_json_path) {
    clean();

    if (!window) {
        spdlog::error("RenderContext::init: 调用时传入的 window 为空。");
        return false;
    }
    window_ = window;

    // 加载配置文件(可能使用默认)
    if (!params_json_path.empty()) {
        if (!loadFromFile(params_json_path)) {
            spdlog::warn("RenderContext::init: 从 '{}' 加载配置文件失败。使用默认设置。", params_json_path);
        }
    }

    // SDL 要求在创建 GL 上下文之前设置属性。如果任何属性失败，我们立即销毁并返回 false。
    if (!setAttributes()) {
        clean();
        return false;
    }

    // 一旦上下文创建成功，我们立即使其当前并加载 GLAD，以便渲染器可以安全地调用 OpenGL 符号。
    if (!createContext()) {
        clean();
        return false;
    }

    if (params_.swap_interval_ != RendererInitParams::SWAP_INTERVAL_DONT_CARE) {
        (void)setSwapInterval(params_.swap_interval_);
    }

    return true;
}

void RenderContext::clean() {
    if (context_) {
        if (window_) {
            SDL_GL_MakeCurrent(window_, nullptr);
        }
        SDL_GL_DestroyContext(context_);
        context_ = nullptr;
    }
    window_ = nullptr;
    params_ = RendererInitParams{};
}

void RenderContext::swapWindow() {
    if (window_) {
        SDL_GL_SwapWindow(window_);
    }
}

bool RenderContext::setSwapInterval(int interval) {
    if (!SDL_GL_SetSwapInterval(interval)) {
        spdlog::warn("SDL_GL_SetSwapInterval({}) failed: {}", interval, SDL_GetError());
        return false;
    }
    return true;
}

int RenderContext::getSwapInterval() const {
    int interval = -1;
    if (!SDL_GL_GetSwapInterval(&interval)) {
        return -1;
    }
    return interval;
}

bool RenderContext::setAttributes() {
    auto requireAttr = [](SDL_GLAttr attr, int value) -> bool {
        if (!SDL_GL_SetAttribute(attr, value)) {
            spdlog::error("SDL_GL_SetAttribute 设置属性失败: attr={}, value={}, error={}", static_cast<int>(attr), value, SDL_GetError());
            return false;
        }
        return true;
    };

    // 单独设置每个属性，以便我们能够报告哪个属性失败——SDL 将大部分逻辑集中在这个步骤中。
    if (!requireAttr(SDL_GL_CONTEXT_MAJOR_VERSION, params_.gl_major_version_) ||
        !requireAttr(SDL_GL_CONTEXT_MINOR_VERSION, params_.gl_minor_version_) ||
        !requireAttr(SDL_GL_CONTEXT_PROFILE_MASK, static_cast<int>(params_.profile_mask_)) ||
        !requireAttr(SDL_GL_CONTEXT_FLAGS, static_cast<int>(params_.context_flags_)) ||
        !requireAttr(SDL_GL_DOUBLEBUFFER, params_.double_buffer_ ? 1 : 0) ||
        !requireAttr(SDL_GL_DEPTH_SIZE, params_.depth_bits_) ||
        !requireAttr(SDL_GL_STENCIL_SIZE, params_.stencil_bits_) ||
        !requireAttr(SDL_GL_MULTISAMPLEBUFFERS, params_.multi_sample_buffers_) ||
        !requireAttr(SDL_GL_MULTISAMPLESAMPLES, params_.multi_sample_samples_) ||
        !requireAttr(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, params_.framebuffer_SRGB_capable_)) {
        return false;
    }

    return true;
}

bool RenderContext::createContext() {
    context_ = SDL_GL_CreateContext(window_);
    if (!context_) {
        spdlog::error("SDL_GL_CreateContext Error: {}", SDL_GetError());
        return false;
    }

    if (!SDL_GL_MakeCurrent(window_, context_)) {
        spdlog::error("SDL_GL_MakeCurrent Error: {}", SDL_GetError());
        SDL_GL_DestroyContext(context_);
        context_ = nullptr;
        return false;
    }

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        spdlog::error("为 RenderContext 初始化 GLAD 失败");
        SDL_GL_MakeCurrent(window_, nullptr);
        SDL_GL_DestroyContext(context_);
        context_ = nullptr;
        return false;
    }
    // 此时上下文已当前且 GLAD 已加载。调用者现在可以安全地发出 OpenGL 命令。

    return true;
}

bool RenderContext::loadFromFile(std::string_view params_json_path) {
    auto path = std::filesystem::path(params_json_path);    // 将string_view转换为文件路径 (或std::sring)
    std::ifstream file(path);                               // ifstream 不支持std::string_view 构造
    if (!file.is_open()) {
        spdlog::warn("配置文件 '{}' 未找到。使用默认设置并创建默认配置文件。", params_json_path);
        return false; // 文件不存在，使用默认值
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json j = nlohmann::json::parse(file_content, nullptr, false);
    if (j.is_discarded()) {
        spdlog::error("读取配置文件 '{}' 时出错：JSON 解析失败。使用默认设置。", params_json_path);
        return false;
    }

    fromJson(j);
    spdlog::info("成功从 '{}' 加载配置。", params_json_path);
    return true;
}

void RenderContext::fromJson(const nlohmann::json& j) {
    if (!j.is_object()) {
        return;
    }

    const auto params_it = j.find("params");
    if (params_it == j.end() || !params_it->is_object()) {
        return;
    }
    const auto& params_config = *params_it;

    auto assignInt = [](const nlohmann::json& obj, std::string_view key, int& target) {
        const auto it = obj.find(key);  // nlohmann::json supports string_view lookup
        if (it == obj.end()) {
            return;
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::number_integer_t*>()) {
            target = static_cast<int>(*v);
            return;
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            target = static_cast<int>(*v);
        }
    };

    auto assignBool = [](const nlohmann::json& obj, std::string_view key, bool& target) {
        const auto it = obj.find(key);  // nlohmann::json supports string_view lookup
        if (it == obj.end()) {
            return;
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::boolean_t*>()) {
            target = *v;
            return;
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::number_integer_t*>()) {
            target = (*v != 0);
            return;
        }
        if (const auto* v = it->get_ptr<const nlohmann::json::number_unsigned_t*>()) {
            target = (*v != 0);
        }
    };

    assignInt(params_config, "gl_major_version", params_.gl_major_version_);
    assignInt(params_config, "gl_minor_version", params_.gl_minor_version_);

    int profile_mask = static_cast<int>(params_.profile_mask_);
    assignInt(params_config, "profile_mask", profile_mask);
    params_.profile_mask_ = static_cast<SDL_GLProfile>(profile_mask);

    int context_flags = static_cast<int>(params_.context_flags_);
    assignInt(params_config, "context_flags", context_flags);
    params_.context_flags_ = static_cast<SDL_GLContextFlag>(context_flags);

    assignBool(params_config, "double_buffer", params_.double_buffer_);
    assignInt(params_config, "depth_bits", params_.depth_bits_);
    assignInt(params_config, "stencil_bits", params_.stencil_bits_);
    assignInt(params_config, "multi_sample_buffers", params_.multi_sample_buffers_);
    assignInt(params_config, "multi_sample_samples", params_.multi_sample_samples_);
    assignInt(params_config, "framebuffer_SRGB_capable", params_.framebuffer_SRGB_capable_);
    assignInt(params_config, "swap_interval", params_.swap_interval_);

    if (params_.swap_interval_ < 0) {         // 如果交换间隔小于0，则设置为 SWAP_INTERVAL_DONT_CARE
        spdlog::warn("交换间隔设置为 SWAP_INTERVAL_DONT_CARE。");
        params_.swap_interval_ = RendererInitParams::SWAP_INTERVAL_DONT_CARE;
    }
}

} // namespace engine::render::opengl
