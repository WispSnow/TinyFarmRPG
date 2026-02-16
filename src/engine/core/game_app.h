#pragma once
#include <memory>
#include <functional>
#include <entt/signal/fwd.hpp>
#include "engine/utils/events.h"

// 前向声明, 减少头文件的依赖，增加编译速度
struct SDL_Window;
struct SDL_Renderer;

namespace engine::resource {
class ResourceManager;
}

namespace engine::render {
class Renderer;
class Camera;
class TextRenderer;
}

namespace engine::render::opengl {
class GLRenderer;
}

namespace engine::input {
class InputManager;
}

namespace engine::scene {
class SceneManager;
}

namespace engine::audio {
class AudioPlayer;
}

namespace engine::debug {
class DebugUIManager;
}

namespace engine::spatial {
class SpatialIndexManager;
}

namespace engine::core {        // 命名空间的最佳实践：与文件路径一致
class Time;
class Config;
class Context;
class GameState;

/**
 * @brief 主游戏应用程序类，初始化SDL，管理游戏循环。
 */
class GameApp final {   // final 表示该类不能被继承
private:
    SDL_Window* window_ = nullptr;
    bool is_running_ = false;

    /// @brief 游戏场景设置函数，用于在运行游戏前设置初始场景 (GameApp不再决定初始场景是什么)
    ///        设计意图：GameApp 属于引擎层，只负责初始化与主循环；
    ///        “第一个场景是什么”属于游戏层决策，由游戏层通过回调把初始 Scene 注入到引擎（通常通过 dispatcher 触发 Push/Replace 事件）。
    std::function<void(engine::core::Context&)> scene_setup_func_;

    // 引擎组件
    std::unique_ptr<entt::dispatcher> dispatcher_;  // 事件分发器
    std::unique_ptr<engine::core::Time> time_;
    std::unique_ptr<engine::resource::ResourceManager> resource_manager_;
    std::unique_ptr<engine::render::Renderer> renderer_;
    std::unique_ptr<engine::render::Camera> camera_;
    std::unique_ptr<engine::render::TextRenderer> text_renderer_;
    std::unique_ptr<engine::core::Config> config_;
    std::unique_ptr<engine::input::InputManager> input_manager_;
    std::unique_ptr<engine::core::Context> context_;
    std::unique_ptr<engine::scene::SceneManager> scene_manager_;
    std::unique_ptr<engine::audio::AudioPlayer> audio_player_;
    std::unique_ptr<engine::core::GameState> game_state_;
    std::unique_ptr<engine::spatial::SpatialIndexManager> spatial_index_manager_;

#ifdef TF_ENABLE_DEBUG_UI
    std::unique_ptr<engine::debug::DebugUIManager> debug_ui_manager_;
#endif
    std::unique_ptr<engine::render::opengl::GLRenderer> gl_renderer_;

public:
    GameApp();
    ~GameApp();

    /**
     * @brief 运行游戏应用程序，其中会调用init()，然后进入主循环，离开循环后自动调用close()。
     */
    void run();

    /**
     * @brief 注册用于设置初始游戏场景的函数。
     *        这个函数将在 SceneManager 初始化后被调用。
     *        GameApp 不直接决定“启动进入哪个场景”，而是由游戏层在这里提供注入逻辑。
     * @param func 一个接收 Context 引用的函数对象（可通过 dispatcher 触发 Push/Replace 场景事件）。
     */
    void registerSceneSetup(std::function<void(engine::core::Context&)> func);

    // 禁止拷贝和移动
    GameApp(const GameApp&) = delete;
    GameApp& operator=(const GameApp&) = delete;
    GameApp(GameApp&&) = delete;
    GameApp& operator=(GameApp&&) = delete;

private:
    [[nodiscard]] bool init();      // nodiscard 表示该函数返回值不应该被忽略
    void handleEvents();
    void update(float delta_time);
    void render();
    void close();

    // 各模块的初始化/创建函数，在init()中调用
    [[nodiscard]] bool initDispatcher();
    [[nodiscard]] bool initConfig();
    [[nodiscard]] bool initSDL();
    [[nodiscard]] bool initGLRenderer();
#ifdef TF_ENABLE_DEBUG_UI
    [[nodiscard]] bool initDebugUIManager();
#endif
    [[nodiscard]] bool initGameState();
    [[nodiscard]] bool initTime();
    [[nodiscard]] bool initResourceManager();
    [[nodiscard]] bool initAudioPlayer();
    [[nodiscard]] bool initRenderer();
    [[nodiscard]] bool initTextRenderer();
    [[nodiscard]] bool initCamera();
    [[nodiscard]] bool initInputManager();
    [[nodiscard]] bool initSpatialIndexManager();
    [[nodiscard]] bool initContext();
    [[nodiscard]] bool initSceneManager();
#ifdef TF_ENABLE_DEBUG_UI
    [[nodiscard]] bool registerDebugPanels();
#endif

    // 事件处理函数
    void onQuitEvent();
    void onWindowResized(const engine::utils::WindowResizedEvent& e);
};

} // namespace engine::core
