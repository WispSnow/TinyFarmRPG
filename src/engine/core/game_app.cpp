#include "engine/core/game_app.h"
#include "engine/core/time.h"
#include "engine/core/context.h"
#include "engine/core/config.h"
#include "engine/core/game_state.h"
#include "engine/resource/resource_manager.h"
#include "engine/audio/audio_player.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/render/text_renderer.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/input/input_manager.h"
#include "engine/scene/scene_manager.h"
#include "engine/utils/events.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#include "engine/debug/panels/time_debug_panel.h"
#include "engine/debug/panels/game_state_debug_panel.h"
#include "engine/debug/panels/input_debug_panel.h"
#include "engine/debug/panels/audio_debug_panel.h"
#include "engine/debug/panels/gl_renderer_debug_panel.h"
#include "engine/debug/panels/res_mgr_debug_panel.h"
#include "engine/debug/panels/text_renderer_debug_panel.h"
#include "engine/debug/panels/ui_preset_debug_panel.h"
#include "engine/debug/panels/scene_debug_panel.h"
#include "engine/debug/panels/spatial_index_debug_panel.h"
#endif
#include "engine/spatial/spatial_index_manager.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

namespace engine::core {

GameApp::GameApp() = default;

GameApp::~GameApp() {
    if (is_running_) {
        spdlog::warn("GameApp 被销毁时没有显式关闭。现在关闭。 ...");
        close();
    }
}

void GameApp::run() {
    if (!init()) {
        spdlog::error("GameApp 初始化失败，无法运行游戏。");
        return;
    }

    while (is_running_) {
        // 每帧节拍（从“玩家输入”到“屏幕呈现”）：
        // 1) 时间推进：计算本帧 deltaTime（用于驱动更新）
        // 2) 输入/事件：轮询 SDL 事件、更新动作状态，并触发必要的引擎事件（例如 Quit/WindowResized）
        // 3) 更新：只更新栈顶场景（以及其系统/逻辑）；场景切换通过事件请求，在 update 末尾统一处理
        // 4) 渲染：清屏 → 渲染场景/调试 UI → present
        // 5) 分发队列事件：处理本帧 enqueue 的事件
        //    TinyFarm 把 dispatcher.update 放在 render 之后：队列事件在“本帧画面已呈现”后才会被分发，
        //    这样可以避免递归触发导致的时序混乱，并把大多数数据类事件自然变成“下一帧的输入”。
        time_->update();
        float delta_time = time_->getDeltaTime();
        
        handleEvents();
        update(delta_time);
        render();

        // 给 tracer 提供一个作用域的标记，用于区分 immediate 和 queued 事件
#ifdef TF_ENABLE_DEBUG_UI
        if (debug_ui_manager_) {
            debug_ui_manager_->onDispatcherUpdateBegin();
        }
#endif
        // 分发 enqueue 的事件（在本项目中相当于“下一轮循环开始前”的事件结算点）
        dispatcher_->update();
#ifdef TF_ENABLE_DEBUG_UI
        if (debug_ui_manager_) {
            debug_ui_manager_->onDispatcherUpdateEnd();
        }
#endif

        // spdlog::info("delta_time: {}", delta_time);
    }

    close();
}

void GameApp::registerSceneSetup(std::function<void(engine::core::Context&)> func)
{
    scene_setup_func_ = std::move(func);
    spdlog::trace("已注册场景设置函数。");
}

bool GameApp::init() {
    spdlog::trace("初始化 GameApp ...");
    if (!scene_setup_func_) {
        spdlog::error("未注册场景设置函数，无法初始化 GameApp。");
        return false;
    }
    if (!initDispatcher()) return false;
    if (!initConfig()) return false;
    if (!initSDL())  return false;
    if (!initGLRenderer()) return false;
#ifdef TF_ENABLE_DEBUG_UI
    if (!initDebugUIManager()) return false;
#endif
    if (!initGameState()) return false;
    if (!initTime()) return false;
    if (!initResourceManager()) return false;
    if (!initAudioPlayer()) return false;
    if (!initRenderer()) return false;
    if (!initCamera()) return false;
    if (!initTextRenderer()) return false;
    if (!initInputManager()) return false;
    if (!initSpatialIndexManager()) return false;

    if (!initContext()) return false;
    if (!initSceneManager()) return false;
#ifdef TF_ENABLE_DEBUG_UI
    if (!registerDebugPanels()) return false;
#endif

    // 注册退出事件 (回调函数可以无参数，代表不使用事件结构体中的数据)
    dispatcher_->sink<utils::QuitEvent>().connect<&GameApp::onQuitEvent>(this);
    // 注册窗口大小变化事件：更新 OpenGL 渲染器视口
    dispatcher_->sink<utils::WindowResizedEvent>().connect<&GameApp::onWindowResized>(this);

    // 调用场景设置函数 (创建第一个场景并压入栈)
    scene_setup_func_(*context_);

    is_running_ = true;
    spdlog::trace("GameApp 初始化成功。");
    return true;
}

void GameApp::handleEvents() {
    // 处理并分发输入事件
    input_manager_->update();
}

void GameApp::update(float delta_time) {
    // 游戏逻辑更新
    scene_manager_->update(delta_time);
}

void GameApp::render() {
    // 1. 清除屏幕
    renderer_->clearScreen();
#ifdef TF_ENABLE_DEBUG_UI
    gl_renderer_->beginDebugUI();
#endif

    // 2. 具体渲染代码
    scene_manager_->render();
#ifdef TF_ENABLE_DEBUG_UI
    gl_renderer_->endDebugUI();
#endif

    // 3. 更新屏幕显示
    renderer_->present();
}

void GameApp::close() {
    spdlog::trace("关闭 GameApp ...");

    // 断开事件处理函数
    dispatcher_->sink<utils::QuitEvent>().disconnect<&GameApp::onQuitEvent>(this);
    dispatcher_->sink<utils::WindowResizedEvent>().disconnect<&GameApp::onWindowResized>(this);

    // 先关闭场景管理器，确保所有场景都被清理
    if (scene_manager_) {
        scene_manager_->close();
        scene_manager_.reset();
    }

    // 释放持有引用关系的 Context，再依次释放依赖的渲染对象
    context_.reset();
    renderer_.reset();
    text_renderer_.reset();
    camera_.reset();

#ifdef TF_ENABLE_DEBUG_UI
    if (gl_renderer_) {
        gl_renderer_->setDebugUIManager(nullptr);
    }
    debug_ui_manager_.reset();
#endif

    // ResourceManager/FontManager/TextureManager 在析构时会释放 GL 资源（glDeleteTextures 等），
    // 必须确保此时 OpenGL 上下文仍然有效，因此要在 GLRenderer 之前销毁。
    resource_manager_.reset();

    // ImGui 依赖的 OpenGL 上下文必须在窗口销毁前清理
    gl_renderer_.reset();

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
    is_running_ = false;
}

bool GameApp::initDispatcher()
{
    dispatcher_ = std::make_unique<entt::dispatcher>();
    spdlog::trace("事件分发器初始化成功。");
    return true;
}

bool GameApp::initConfig()
{
    config_ = engine::core::Config::create("config/window.json");
    if (!config_) {
        spdlog::error("初始化配置失败。");
        return false;
    }
    spdlog::trace("配置初始化成功。");
    return true;
}

bool GameApp::initSDL()
{
    SDL_SetHint("SDL_HINT_IME_SHOW_UI", "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        spdlog::error("SDL 初始化失败! SDL错误: {}", SDL_GetError());
        return false;
    }

    // 设置窗口大小 (窗口大小 * 窗口缩放比例)
    int window_width = static_cast<int>(static_cast<float>(config_->window_width_) * config_->window_scale_);
    int window_height = static_cast<int>(static_cast<float>(config_->window_height_) * config_->window_scale_);
    window_ = SDL_CreateWindow(config_->window_title_.c_str(), window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        spdlog::error("无法创建窗口! SDL错误: {}", SDL_GetError());
        return false;
    }

    return true;
}

bool GameApp::initGLRenderer() {
    // 获取逻辑分辨率 (窗口大小 * 逻辑缩放比例)
    // 💡 为什么要单独计算 logical_size？
    //   - logical_size 定义离屏渲染 FBO 的分辨率（固定的"设计分辨率"）
    //   - 无论窗口怎么变化，逻辑分辨率保持一致，所以 UI 和相机计算也保持一致
    //   - window_scale 只影响窗口初始大小；logical_scale 才决定渲染质量
    auto logical_size = glm::vec2(config_->window_width_ * config_->window_logical_scale_,
                                  config_->window_height_ * config_->window_logical_scale_);

    if (gl_renderer_ = engine::render::opengl::GLRenderer::create(window_, logical_size, "config/render.json"); !gl_renderer_) {
        spdlog::error("初始化 OpenGL 渲染器失败。");
        return false;
    }
    gl_renderer_->setVSyncEnabled(config_->vsync_enabled_);
    gl_renderer_->setDebugUIEnabled(config_->debug_ui_enabled_);
    spdlog::trace("OpenGL 渲染器初始化成功。");
    return true;
}

#ifdef TF_ENABLE_DEBUG_UI
bool GameApp::initDebugUIManager() {
    debug_ui_manager_ = std::make_unique<engine::debug::DebugUIManager>();
    if (config_ && config_->debug_ui_enabled_ && dispatcher_) {
        debug_ui_manager_->enableDispatcherTrace(*dispatcher_);
    }
    if (gl_renderer_) {
        gl_renderer_->setDebugUIManager(debug_ui_manager_.get());
    }
    return true;
}

bool GameApp::registerDebugPanels() {
    if (!debug_ui_manager_) {
        spdlog::warn("DebugUIManager 未初始化，跳过调试面板注册。");
        return true;
    }

    debug_ui_manager_->registerPanel(std::make_unique<engine::render::opengl::GLRendererDebugPanel>(*gl_renderer_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::TimeDebugPanel>(*time_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::GameStateDebugPanel>(*game_state_, *camera_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::InputDebugPanel>(*input_manager_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::AudioDebugPanel>(*audio_player_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::TextRendererDebugPanel>(*text_renderer_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::ResMgrDebugPanel>(*resource_manager_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::UIPresetDebugPanel>(*resource_manager_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::SceneDebugPanel>(*scene_manager_), false, engine::debug::PanelCategory::Engine);
    debug_ui_manager_->registerPanel(std::make_unique<engine::debug::SpatialIndexDebugPanel>(*scene_manager_, *spatial_index_manager_), false, engine::debug::PanelCategory::Engine);
    return true;
}
#endif

bool GameApp::initGameState()
{
    game_state_ = engine::core::GameState::create(window_);
    if (!game_state_) {
        spdlog::error("初始化游戏状态失败。");
        return false;
    }
    if (config_) {
        const auto logical_size = glm::vec2(
            config_->window_width_ * config_->window_logical_scale_,
            config_->window_height_ * config_->window_logical_scale_);
        game_state_->setLogicalSize(logical_size);
    }
    return true;
}

bool GameApp::initTime() {
    time_ = std::make_unique<engine::core::Time>();
    time_->setTargetFps(config_->target_fps_);
    spdlog::trace("时间管理初始化成功。");
    return true;
}

bool GameApp::initResourceManager() {
    resource_manager_ = engine::resource::ResourceManager::create(dispatcher_.get());
    if (!resource_manager_) {
        spdlog::error("初始化资源管理器失败。");
        return false;
    }
    spdlog::trace("资源管理器初始化成功。");
    resource_manager_->loadResources("assets/data/resource_mapping.json");  // 载入默认资源映射文件
    return true;
}

bool GameApp::initAudioPlayer()
{
    audio_player_ = engine::audio::AudioPlayer::create(resource_manager_.get());
    if (!audio_player_) {
        spdlog::error("初始化音频播放器失败。");
        return false;
    }
    spdlog::trace("音频播放器初始化成功。");
    return true;
}

bool GameApp::initRenderer() {
    renderer_ = engine::render::Renderer::create(gl_renderer_.get(), resource_manager_.get());
    if (!renderer_) {
        spdlog::error("初始化渲染器失败。");
        return false;
    }
    spdlog::trace("渲染器初始化成功。");
    return true;
}

bool GameApp::initCamera() {
    camera_ = std::make_unique<engine::render::Camera>(game_state_->getLogicalSize());
    spdlog::trace("相机初始化成功。");
    return true;
}

bool GameApp::initTextRenderer()
{
    text_renderer_ = engine::render::TextRenderer::create(gl_renderer_.get(), resource_manager_.get(), dispatcher_.get());
    if (!text_renderer_) {
        spdlog::error("初始化文字渲染引擎失败。");
        return false;
    }
    spdlog::trace("文字渲染引擎初始化成功。");
    return true;
}

bool GameApp::initInputManager()
{
    input_manager_ = engine::input::InputManager::create(dispatcher_.get(), game_state_.get());
    if (!input_manager_) {
        spdlog::error("初始化输入管理器失败。");
        return false;
    }
#ifdef TF_ENABLE_DEBUG_UI
    input_manager_->setImGuiEventForwarder([this](const SDL_Event& event) {
        if (gl_renderer_) {
            gl_renderer_->handleSDLEvent(event);
        }
    });
#endif
    spdlog::trace("输入管理器初始化成功。");
    return true;
}

bool GameApp::initSpatialIndexManager()
{
    spatial_index_manager_ = std::make_unique<engine::spatial::SpatialIndexManager>();
    return true;
}

bool GameApp::initContext()
{
    context_ = engine::core::Context::create(*dispatcher_,
                                             *input_manager_,
                                             *gl_renderer_,
                                             *renderer_,
                                             *camera_,
                                             *text_renderer_,
                                             *resource_manager_,
                                             *audio_player_,
                                             *game_state_,
                                             *time_,
#ifdef TF_ENABLE_DEBUG_UI
                                             *debug_ui_manager_,
#endif
                                             *spatial_index_manager_);
    if (!context_) {
        spdlog::error("初始化上下文失败。");
        return false;
    }
    return true;
}

bool GameApp::initSceneManager()
{
    scene_manager_ = std::make_unique<engine::scene::SceneManager>(*context_);
    spdlog::trace("场景管理器初始化成功。");
    return true;
}

void GameApp::onQuitEvent()
{
    spdlog::trace("GameApp 收到来自事件分发器的退出请求。");
    is_running_ = false;
}

void GameApp::onWindowResized(const engine::utils::WindowResizedEvent& e)
{
    // 使用像素大小更新 OpenGL 视口（高 DPI 下，window 坐标尺寸 ≠ drawable 像素尺寸）
    int w = e.width;
    int h = e.height;
    if (!e.pixel_size) {
        // 将窗口坐标转换为像素坐标（高DPI）
        SDL_GetWindowSizeInPixels(window_, &w, &h);
    }
    if (gl_renderer_) {
        gl_renderer_->resize(w, h);
    }
}

} // namespace engine::core
