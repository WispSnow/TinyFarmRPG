#pragma once
#include <entt/signal/fwd.hpp>

namespace engine::input {
    class InputManager;
}

namespace engine::render {
    class Renderer;
    class Camera;
    class TextRenderer;
}

namespace engine::render::opengl {
    class GLRenderer;
}

namespace engine::resource {
    class ResourceManager;
    class AutoTileLibrary;
}

namespace engine::ui {
    class UIPresetManager;
}

namespace engine::async {
    class MainThreadCommandQueue;
}

namespace engine::core {
    class GameState;
    class Time;

/// @brief 核心基础设施服务（事件、状态、输入、时间、异步队列）
struct CoreServices {
    entt::dispatcher& dispatcher;
    engine::core::GameState& game_state;
    engine::core::Time& time;
    engine::input::InputManager& input_manager;
    engine::async::MainThreadCommandQueue& main_thread_command_queue;
};

/// @brief 渲染相关服务（GL渲染器、高层渲染器、相机、文本渲染）
struct RenderServices {
    engine::render::opengl::GLRenderer& gl_renderer;
    engine::render::Renderer& renderer;
    engine::render::Camera& camera;
    engine::render::TextRenderer& text_renderer;
};

/// @brief 资源/资产服务（资源管理器、自动图块、UI预设）
struct ResourceServices {
    engine::resource::ResourceManager& resource_manager;
    engine::resource::AutoTileLibrary& auto_tile_library;
    engine::ui::UIPresetManager& ui_preset_manager;
};

} // namespace engine::core
