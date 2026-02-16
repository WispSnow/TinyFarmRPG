#include "engine/core/context.h"
#include "engine/core/time.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/render/text_renderer.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/audio/audio_player.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif
#include "engine/spatial/spatial_index_manager.h"
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

namespace engine::core {

std::unique_ptr<Context> Context::create(entt::dispatcher& dispatcher,
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
                                        engine::spatial::SpatialIndexManager& spatial_index_manager) {
    return std::unique_ptr<Context>(
        new Context(dispatcher,
                    input_manager,
                    gl_renderer,
                    renderer,
                    camera,
                    text_renderer,
                    resource_manager,
                    audio_player,
                    game_state,
                    time,
#ifdef TF_ENABLE_DEBUG_UI
                    debug_ui_manager,
#endif
                    spatial_index_manager)
    );
}

Context::Context(entt::dispatcher& dispatcher,
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
                 engine::spatial::SpatialIndexManager& spatial_index_manager)
    : dispatcher_(dispatcher),
      input_manager_(input_manager),
      gl_renderer_(gl_renderer),
      renderer_(renderer),
      camera_(camera),
      text_renderer_(text_renderer),
      resource_manager_(resource_manager),
      audio_player_(audio_player),
      game_state_(game_state),
      time_(time),
#ifdef TF_ENABLE_DEBUG_UI
      debug_ui_manager_(debug_ui_manager),
#endif
      spatial_index_manager_(spatial_index_manager)
{
    spdlog::trace("上下文已创建并初始化。");
}

} // namespace engine::core 
