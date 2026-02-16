# ============================================
# 项目源文件
# ============================================
# 使用 target_sources() 为根 CMakeLists.txt 中定义的目标填充源文件。
# 目标（engine / game / ${TARGET}）已在根文件中创建。

# ============================================
# Engine 源文件
# ============================================

target_sources(engine PRIVATE
    # Audio
    engine/audio/audio_player.cpp
    engine/audio/stb_vorbis_impl.c
    # Core
    engine/core/game_app.cpp
    engine/core/time.cpp
    engine/core/config.cpp
    engine/core/context.cpp
    engine/core/game_state.cpp
    # Resource
    engine/resource/resource_manager.cpp
    engine/resource/texture_manager.cpp
    engine/resource/audio_manager.cpp
    engine/resource/font_manager.cpp
    engine/resource/auto_tile_library.cpp
    # Render
    engine/render/renderer.cpp
    engine/render/image.cpp
    engine/render/camera.cpp
    engine/render/text_renderer.cpp
    engine/render/nine_slice.cpp
    # Render - OpenGL
    engine/render/opengl/render_context.cpp
    engine/render/opengl/viewport_manager.cpp
    engine/render/opengl/gl_renderer.cpp
    engine/render/opengl/sprite_batch.cpp
    engine/render/opengl/shader_program.cpp
    engine/render/opengl/shader_library.cpp
    engine/render/opengl/composite_pass.cpp
    engine/render/opengl/lighting_pass.cpp
    engine/render/opengl/emissive_pass.cpp
    engine/render/opengl/bloom_pass.cpp
    engine/render/opengl/scene_pass.cpp
    engine/render/opengl/ui_pass.cpp
    # Input
    engine/input/input_manager.cpp
    # Loader
    engine/loader/level_loader.cpp
    engine/loader/basic_entity_builder.cpp
    # Scene
    engine/scene/scene.cpp
    engine/scene/scene_manager.cpp
    # System
    engine/system/audio_system.cpp
    engine/system/animation_system.cpp
    engine/system/auto_tile_system.cpp
    engine/system/render_system.cpp
    engine/system/light_system.cpp
    engine/system/movement_system.cpp
    engine/system/ysort_system.cpp
    engine/system/spatial_index_system.cpp
    engine/system/remove_entity_system.cpp
    # Spatial
    engine/spatial/static_tile_grid.cpp
    engine/spatial/dynamic_entity_grid.cpp
    engine/spatial/spatial_index_manager.cpp
    engine/spatial/collision_resolver.cpp
    # UI
    engine/ui/ui_manager.cpp
    engine/ui/ui_element.cpp
    engine/ui/ui_interactive.cpp
    engine/ui/ui_input_blocker.cpp
    engine/ui/ui_panel.cpp
    engine/ui/ui_image.cpp
    engine/ui/ui_label.cpp
    engine/ui/ui_button.cpp
    engine/ui/ui_draggable_panel.cpp
    engine/ui/ui_preset_manager.cpp
    engine/ui/ui_drag_preview.cpp
    engine/ui/ui_screen_fade.cpp
    engine/ui/state/ui_normal_state.cpp
    engine/ui/state/ui_pressed_state.cpp
    engine/ui/state/ui_hover_state.cpp
    # UI - Layout
    engine/ui/layout/ui_layout.cpp
    engine/ui/layout/ui_stack_layout.cpp
    engine/ui/layout/ui_grid_layout.cpp
    engine/ui/ui_progress_bar.cpp
    engine/ui/ui_item_slot.cpp
)

# Engine Debug UI 源文件（仅在启用 Debug UI 时编译）
if(ENABLE_DEBUG_UI)
    target_sources(engine PRIVATE
        engine/debug/debug_ui_manager.cpp
        engine/debug/dispatcher_trace.cpp
        engine/debug/panels/dispatcher_trace_debug_panel.cpp
        engine/debug/panels/time_debug_panel.cpp
        engine/debug/panels/game_state_debug_panel.cpp
        engine/debug/panels/input_debug_panel.cpp
        engine/debug/panels/audio_debug_panel.cpp
        engine/debug/panels/text_renderer_debug_panel.cpp
        engine/debug/panels/res_mgr_debug_panel.cpp
        engine/debug/panels/ui_preset_debug_panel.cpp
        engine/debug/panels/scene_debug_panel.cpp
        engine/debug/panels/spatial_index_debug_panel.cpp
        engine/debug/panels/gl_renderer_debug_panel.cpp
        engine/render/opengl/imgui_layer.cpp
        engine/system/debug_render_system.cpp
    )
endif()

# ============================================
# Game 源文件
# ============================================

target_sources(game PRIVATE
    # Entry
    game/game_entry.cpp
    # Component
    game/component/state_component.h
    game/component/npc_component.h
    game/component/chest_component.h
    game/component/pickup_component.h
    # Data
    game/data/game_time.cpp
    game/data/item_catalog.cpp
    # Save
    game/save/save_data.cpp
    game/save/save_slot_summary.cpp
    game/save/save_service.cpp
    # Factory
    game/factory/blueprint_manager.cpp
    game/factory/entity_factory.cpp
    # Loader
    game/loader/entity_builder.cpp
    # UI
    game/ui/time_clock_ui.cpp
    game/ui/inventory_ui.cpp
    game/ui/hotbar_ui.cpp
    game/ui/dialogue_bubble.cpp
    game/ui/item_tooltip_ui.cpp
    # System
    game/system/state_system.cpp
    game/system/action_sound_system.cpp
    game/system/player_control_system.cpp
    game/system/camera_follow_system.cpp
    game/system/farm_system.cpp
    game/system/render_target_system.cpp
    game/system/animation_event_system.cpp
    game/system/time_system.cpp
    game/system/time_of_day_light_system.cpp
    game/system/day_night_system.cpp
    game/system/light_toggle_system.cpp
    game/system/crop_system.cpp
    game/system/inventory_system.cpp
    game/system/hotbar_system.cpp
    game/system/item_use_system.cpp
    game/system/npc_wander_system.cpp
    game/system/animal_behavior_system.cpp
    game/system/dialogue_system.cpp
    game/system/chest_system.cpp
    game/system/interaction_system.cpp
    game/system/map_transition_system.cpp
    game/system/pickup_system.cpp
    game/system/rest_system.cpp
    # World
    game/world/world_state.cpp
    game/world/map_manager.cpp
    game/world/map_loading_settings.cpp
    game/world/map_snapshot_serializer.cpp
    # Scene
    game/scene/game_scene.cpp
    game/scene/title_scene.cpp
    game/scene/save_slot_select_scene.cpp
    game/scene/pause_menu_scene.cpp
    game/scene/rest_dialog_scene.cpp
)

# Game Debug 面板（仅在启用 Debug UI 时编译）
if(ENABLE_DEBUG_UI)
    target_sources(game PRIVATE
        game/debug/player_debug_panel.cpp
        game/debug/inventory_debug_panel.cpp
        game/debug/game_time_debug_panel.cpp
        game/debug/save_load_debug_panel.cpp
        game/debug/map_inspector_debug_panel.cpp
        game/debug/blueprint_inspector_debug_panel.cpp
    )
endif()

# ============================================
# 可执行文件源文件
# ============================================

target_sources(${TARGET} PRIVATE
    main.cpp
)

# Windows 平台添加资源文件
if(WIN32)
    target_sources(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/resources.rc
    )
endif()
