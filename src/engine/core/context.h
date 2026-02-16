#pragma once
#include <entt/signal/fwd.hpp>
#include <memory>
// 前置声明核心系统
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
}

namespace engine::audio {
    class AudioPlayer;
}

#ifdef TF_ENABLE_DEBUG_UI
namespace engine::debug {
    class DebugUIManager;
}
#endif

namespace engine::spatial {
    class SpatialIndexManager;
}

namespace engine::core {
    class GameState;
    class Time;

/**
 * @brief 持有对核心引擎模块引用的上下文对象。
 *
 * 用于简化依赖注入，传递Context即可获取引擎的各个模块。
 */
class Context final {
private:
    // 使用引用，确保每个模块都有效，使用时不需要检查指针是否为空。
    entt::dispatcher& dispatcher_;                          ///< @brief 事件分发器
    engine::input::InputManager& input_manager_;            ///< @brief 输入管理器
    engine::render::opengl::GLRenderer& gl_renderer_;       ///< @brief OpenGL渲染器
    engine::render::Renderer& renderer_;                    ///< @brief 渲染器
    engine::render::Camera& camera_;                        ///< @brief 相机
    engine::render::TextRenderer& text_renderer_;           ///< @brief 文本渲染器
    engine::resource::ResourceManager& resource_manager_;   ///< @brief 资源管理器
    engine::audio::AudioPlayer& audio_player_;              ///< @brief 音频播放器
    engine::core::GameState& game_state_;                   ///< @brief 游戏状态
    engine::core::Time& time_;                              ///< @brief 时间
#ifdef TF_ENABLE_DEBUG_UI
    engine::debug::DebugUIManager& debug_ui_manager_;       ///< @brief 调试面板管理器
#endif
    engine::spatial::SpatialIndexManager& spatial_index_manager_; ///< @brief 空间索引管理器

    Context(entt::dispatcher& dispatcher,
            engine::input::InputManager& input_manager,
            engine::render::opengl::GLRenderer& gl_renderer,
            engine::render::Renderer& renderer,
            engine::render::Camera& camera,
            engine::render::TextRenderer& text_renderer,
            engine::resource::ResourceManager& resource_manager,
            engine::audio::AudioPlayer& audio_player,
            engine::core::GameState& game_state,
            engine::core::Time& time,
#ifdef TF_ENABLE_DEBUG_UI
            engine::debug::DebugUIManager& debug_ui_manager,
#endif
            engine::spatial::SpatialIndexManager& spatial_index_manager);
public:
    /**
     * @brief 创建 Context 实例。
     */
    [[nodiscard]] static std::unique_ptr<Context> create(entt::dispatcher& dispatcher,
                                                         engine::input::InputManager& input_manager,
                                                         engine::render::opengl::GLRenderer& gl_renderer,
                                                         engine::render::Renderer& renderer,
                                                         engine::render::Camera& camera,
                                                         engine::render::TextRenderer& text_renderer,
                                                         engine::resource::ResourceManager& resource_manager,
                                                         engine::audio::AudioPlayer& audio_player,
                                                         engine::core::GameState& game_state,
                                                         engine::core::Time& time,
#ifdef TF_ENABLE_DEBUG_UI
                                                         engine::debug::DebugUIManager& debug_ui_manager,
#endif
                                                         engine::spatial::SpatialIndexManager& spatial_index_manager);

    // 禁止拷贝和移动，Context 对象通常是唯一的或按需创建/传递
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    // --- Getters ---
    [[nodiscard]] entt::dispatcher& getDispatcher() const { return dispatcher_; }
    [[nodiscard]] engine::input::InputManager& getInputManager() const { return input_manager_; }
    [[nodiscard]] engine::render::opengl::GLRenderer& getGLRenderer() const { return gl_renderer_; }
    [[nodiscard]] engine::render::Renderer& getRenderer() const { return renderer_; }
    [[nodiscard]] engine::render::Camera& getCamera() const { return camera_; }
    [[nodiscard]] engine::render::TextRenderer& getTextRenderer() const { return text_renderer_; }
    [[nodiscard]] engine::resource::ResourceManager& getResourceManager() const { return resource_manager_; }
    [[nodiscard]] engine::audio::AudioPlayer& getAudioPlayer() const { return audio_player_; }
    [[nodiscard]] engine::core::GameState& getGameState() const { return game_state_; }
    [[nodiscard]] engine::core::Time& getTime() const { return time_; }
#ifdef TF_ENABLE_DEBUG_UI
    [[nodiscard]] engine::debug::DebugUIManager& getDebugUIManager() const { return debug_ui_manager_; }
#endif
    [[nodiscard]] engine::spatial::SpatialIndexManager& getSpatialIndexManager() const { return spatial_index_manager_; }
};

} // namespace engine::core
