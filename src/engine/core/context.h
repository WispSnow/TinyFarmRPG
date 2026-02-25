#pragma once
#include "engine/core/context_modules.h"
#include <memory>

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

/**
 * @brief 持有对核心引擎模块引用的上下文对象。
 *
 * 用于简化依赖注入，传递Context即可获取引擎的各个模块。
 * 服务按职责分组为 CoreServices / RenderServices / ResourceServices 三个模块。
 */
class Context final {
private:
    CoreServices     core_;
    RenderServices   render_;
    ResourceServices resource_;

    engine::audio::AudioPlayer& audio_player_;
    engine::spatial::SpatialIndexManager& spatial_index_manager_;
#ifdef TF_ENABLE_DEBUG_UI
    engine::debug::DebugUIManager& debug_ui_manager_;
#endif

    Context(CoreServices core,
            RenderServices render,
            ResourceServices resource,
            engine::audio::AudioPlayer& audio_player,
            engine::spatial::SpatialIndexManager& spatial_index_manager
#ifdef TF_ENABLE_DEBUG_UI
            , engine::debug::DebugUIManager& debug_ui_manager
#endif
            );

public:
    /**
     * @brief 创建 Context 实例。
     */
    [[nodiscard]] static std::unique_ptr<Context> create(
            CoreServices core,
            RenderServices render,
            ResourceServices resource,
            engine::audio::AudioPlayer& audio_player,
            engine::spatial::SpatialIndexManager& spatial_index_manager
#ifdef TF_ENABLE_DEBUG_UI
            , engine::debug::DebugUIManager& debug_ui_manager
#endif
            );

    // 禁止拷贝和移动，Context 对象通常是唯一的或按需创建/传递
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    // --- 模块级 Getters ---
    [[nodiscard]] const CoreServices&     core()     const { return core_; }
    [[nodiscard]] const RenderServices&   render()   const { return render_; }
    [[nodiscard]] const ResourceServices& resource() const { return resource_; }

    // --- 便捷 Getters（转发至模块成员，保持现有调用点兼容）---
    [[nodiscard]] entt::dispatcher& getDispatcher() const { return core_.dispatcher; }
    [[nodiscard]] engine::input::InputManager& getInputManager() const { return core_.input_manager; }
    [[nodiscard]] engine::core::GameState& getGameState() const { return core_.game_state; }
    [[nodiscard]] engine::core::Time& getTime() const { return core_.time; }
    [[nodiscard]] engine::async::MainThreadCommandQueue& getMainThreadCommandQueue() const { return core_.main_thread_command_queue; }

    [[nodiscard]] engine::render::opengl::GLRenderer& getGLRenderer() const { return render_.gl_renderer; }
    [[nodiscard]] engine::render::Renderer& getRenderer() const { return render_.renderer; }
    [[nodiscard]] engine::render::Camera& getCamera() const { return render_.camera; }
    [[nodiscard]] engine::render::TextRenderer& getTextRenderer() const { return render_.text_renderer; }

    [[nodiscard]] engine::resource::ResourceManager& getResourceManager() const { return resource_.resource_manager; }
    [[nodiscard]] engine::resource::AutoTileLibrary& getAutoTileLibrary() const { return resource_.auto_tile_library; }
    [[nodiscard]] engine::ui::UIPresetManager& getUIPresetManager() const { return resource_.ui_preset_manager; }

    [[nodiscard]] engine::audio::AudioPlayer& getAudioPlayer() const { return audio_player_; }
    [[nodiscard]] engine::spatial::SpatialIndexManager& getSpatialIndexManager() const { return spatial_index_manager_; }
#ifdef TF_ENABLE_DEBUG_UI
    [[nodiscard]] engine::debug::DebugUIManager& getDebugUIManager() const { return debug_ui_manager_; }
#endif
};

} // namespace engine::core
