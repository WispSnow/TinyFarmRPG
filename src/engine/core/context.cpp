#include "engine/core/context.h"
#include "engine/core/time.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/render/text_renderer.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/audio/audio_player.h"
#include "engine/ui/ui_preset_manager.h"
#include "engine/async/main_thread_command_queue.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif
#include "engine/spatial/spatial_index_manager.h"
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

namespace engine::core {

std::unique_ptr<Context> Context::create(
        CoreServices core,
        RenderServices render,
        ResourceServices resource,
        engine::audio::AudioPlayer& audio_player,
        engine::spatial::SpatialIndexManager& spatial_index_manager
#ifdef TF_ENABLE_DEBUG_UI
        , engine::debug::DebugUIManager& debug_ui_manager
#endif
        ) {
    return std::unique_ptr<Context>(
        new Context(core,
                    render,
                    resource,
                    audio_player,
                    spatial_index_manager
#ifdef TF_ENABLE_DEBUG_UI
                    , debug_ui_manager
#endif
                    )
    );
}

Context::Context(CoreServices core,
                 RenderServices render,
                 ResourceServices resource,
                 engine::audio::AudioPlayer& audio_player,
                 engine::spatial::SpatialIndexManager& spatial_index_manager
#ifdef TF_ENABLE_DEBUG_UI
                 , engine::debug::DebugUIManager& debug_ui_manager
#endif
                 )
    : core_(core),
      render_(render),
      resource_(resource),
      audio_player_(audio_player),
      spatial_index_manager_(spatial_index_manager)
#ifdef TF_ENABLE_DEBUG_UI
      , debug_ui_manager_(debug_ui_manager)
#endif
{
    spdlog::trace("上下文已创建并初始化。");
}

} // namespace engine::core
