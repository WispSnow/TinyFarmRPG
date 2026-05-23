# ============================================
# 项目源文件
# ============================================
# 使用 target_sources() 为根 CMakeLists.txt 中定义的目标填充源文件。
# 目标（engine / game / ${TARGET}）已在根文件中创建。

# ============================================
# Engine 源文件
# ============================================

target_sources(engine PRIVATE
    # Async
    engine/async/thread_pool.cpp
    engine/async/main_thread_command_queue.cpp
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
    engine/resource/asset_registry.cpp
    engine/resource/resource_manager.cpp
    engine/resource/image_decode_service.cpp
    engine/resource/stb_image_mutex.cpp
    engine/resource/font_preprocess_service.cpp
    engine/resource/texture_loader.cpp
    engine/resource/texture_manager.cpp
    engine/resource/audio_loader.cpp
    engine/resource/audio_manager.cpp
    engine/resource/font_manager.cpp
    engine/resource/auto_tile_library.cpp
    # Render
    engine/render/renderer.cpp
    engine/render/image.cpp
    engine/render/camera.cpp
    engine/render/text_renderer.cpp
    engine/render/nine_slice.cpp
    # VFX
    engine/vfx/null_vfx_backend.cpp
    engine/vfx/vfx_service.cpp
    engine/vfx/vfx_catalog.cpp
    engine/vfx/vfx_bridge_system.cpp
    # Render - OpenGL
    engine/render/opengl/gl_helper.cpp
    engine/render/opengl/fullscreen_quad.cpp
    engine/render/opengl/default_textures.cpp
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
    engine/render/opengl/world_vfx_pass.cpp
    engine/render/opengl/vfx_pass.cpp
    # Input
    engine/input/input_buffer.cpp
    engine/input/input_glyphs.cpp
    engine/input/input_manager.cpp
    # Loader
    engine/loader/level_loader.cpp
    engine/loader/level_preprocess_service.cpp
    engine/loader/basic_entity_builder.cpp
    # Scene
    engine/scene/scene.cpp
    engine/scene/scene_manager.cpp
    # System
    engine/system/audio_system.cpp
    engine/system/animation_system.cpp
    engine/system/auto_tile_system.cpp
    engine/system/deferred_commands.cpp
    engine/system/task_event_buffer.cpp
    engine/system/parallel_wave_scheduler.cpp
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
        engine/debug/panels/scene_debug_panel.cpp
        engine/debug/panels/spatial_index_debug_panel.cpp
        engine/debug/panels/gl_renderer_debug_panel.cpp
        engine/debug/panels/vfx_debug_panel.cpp
        engine/debug/panels/rmlui_debug_panel.cpp
        engine/render/opengl/imgui_layer.cpp
        engine/system/debug_render_system.cpp
    )
endif()

target_sources(engine PRIVATE
    engine/script/script_entity_handle.cpp
    engine/script/script_host.cpp
    engine/script/script_binding_utils.cpp
)

target_sources(engine PRIVATE
    engine/vfx/effekseer_backend.cpp
    engine/vfx/effekseer_backend_factory.cpp
)

set(_rmlui_source_root "${CMAKE_SOURCE_DIR}/external/RmlUi-6.2")
if(DEFINED RMLUI_SOURCE_DIR_RESOLVED AND EXISTS "${RMLUI_SOURCE_DIR_RESOLVED}")
    set(_rmlui_source_root "${RMLUI_SOURCE_DIR_RESOLVED}")
endif()

set(_rmlui_platform_sdl_src "${_rmlui_source_root}/Backends/RmlUi_Platform_SDL.cpp")
set(_rmlui_renderer_gl3_src "${_rmlui_source_root}/Backends/RmlUi_Renderer_GL3.cpp")

target_sources(engine PRIVATE
    ${_rmlui_platform_sdl_src}
    ${_rmlui_renderer_gl3_src}
    engine/ui/rmlui/rml_ui_runtime.cpp
    engine/ui/rmlui/rml_ui_render_backend_gl.cpp
    engine/ui/rmlui/rml_data_bridge.cpp
    engine/ui/rmlui/rml_document_controller.cpp
    engine/ui/rmlui/rml_generated_image_registry.cpp
    engine/ui/rmlui/rml_screen_fade.cpp
    engine/ui/rmlui/render_interface_gl3_stb.cpp
)

target_include_directories(engine PRIVATE
    "${_rmlui_source_root}/Backends"
)

target_compile_definitions(engine PRIVATE
    RMLUI_SDL_VERSION_MAJOR=3
    RMLUI_GL3_CUSTOM_LOADER=<glad/glad.h>
)

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
    game/data/rpg_types.cpp
    game/data/rpg_catalog.cpp
    game/data/audio_cue_catalog.cpp
    game/data/appearance_catalog.cpp
    game/data/item_catalog.cpp
    game/data/quest_catalog.cpp
    game/data/shop_catalog.cpp
    # Battle
    game/battle/battle_action_resolver.cpp
    game/battle/actor_stats_resolver.cpp
    game/battle/battle_ai_planner.cpp
    game/battle/battle_log_formatter.cpp
    game/battle/battle_reward_resolver.cpp
    game/battle/battle_unit_factory.cpp
    game/battle/battle_formula_evaluator.cpp
    game/battle/turn_core.cpp
    game/battle/battle_session.cpp
    # Domain
    game/domain/inventory_domain_service.cpp
    game/domain/equipment_domain_service.cpp
    game/domain/actor_progression_service.cpp
    game/domain/quest_battle_progress_resolver.cpp
    game/domain/quest_log_ops.cpp
    game/domain/quest_turn_in_service.cpp
    game/domain/shop_transaction_service.cpp
    # Debug helpers
    game/debug/quest_debug_panel_helpers.cpp
    game/debug/shop_debug_panel_helpers.cpp
    # Save
    game/save/save_data.cpp
    game/save/save_migrator.cpp
    game/save/save_slot_summary.cpp
    game/save/save_service.cpp
    # Factory
    game/factory/blueprint_manager.cpp
    game/factory/entity_factory.cpp
    # Loader
    game/loader/entity_builder.cpp
    # Runtime
    game/runtime/asset_preload_registrar.cpp
    game/runtime/content_catalog_loader.cpp
    game/runtime/system_bundle.cpp
    game/runtime/game_runtime_assembler.cpp
    game/runtime/runtime_service_factory.cpp
    game/runtime/rpg_catalog_loader.cpp
    game/runtime/script_runtime_factory.cpp
    game/runtime/system_factory.cpp
    game/runtime/system_scheduler.cpp
    game/runtime/user_settings.cpp
    game/runtime/user_settings_service.cpp
    # UI
    game/ui/time_clock_hud.cpp
    game/ui/game_overlay.cpp
    game/ui/game_input_prompt_overlay.cpp
    game/ui/hotbar_ui.cpp
    game/ui/slot_grid_support.cpp
    game/ui/shop_menu_support.cpp
    game/ui/inventory_menu_tab_shortcuts.cpp
    game/ui/appearance_customize_view_model.cpp
    game/ui/text_utils.cpp
    game/ui/inventory_tab_content.cpp
    game/ui/equipment_tab_content.cpp
    game/ui/options_tab_content.cpp
    game/ui/map_coordinate_mapper.cpp
    game/ui/map_marker_provider.cpp
    game/ui/map_preview_builder.cpp
    game/ui/map_tab_content.cpp
    game/ui/quest_map_marker_resolver.cpp
    game/ui/quest_tab_content.cpp
    game/ui/dialogue_box_view.cpp
    game/ui/dialogue_presentation_controller.cpp
    game/ui/floating_notice_view.cpp
    game/ui/game_scene_ui_controller.cpp
    game/ui/item_tooltip_ui.cpp
    # System
    game/system/state_system.cpp
    game/system/action_sound_system.cpp
    game/system/appearance_layer_cache_builder.cpp
    game/system/appearance_system.cpp
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
    game/system/equipment_system.cpp
    game/system/hotbar_system.cpp
    game/system/item_use_system.cpp
    game/system/npc_wander_system.cpp
    game/system/animal_behavior_system.cpp
    game/system/wander_runtime.cpp
    game/system/dialogue_system.cpp
    game/system/quest_interaction_system.cpp
    game/system/recruitment_interaction_system.cpp
    game/system/party_recruitment_system.cpp
    game/system/shop_interaction_system.cpp
    game/system/enemy_encounter_system.cpp
    game/system/chest_system.cpp
    game/system/closet_interaction_system.cpp
    game/system/interaction_system.cpp
    game/system/map_transition_system.cpp
    game/system/pickup_system.cpp
    game/system/rest_system.cpp
    # World
    game/world/world_state.cpp
    game/world/async_preload_pipeline.cpp
    game/world/map_manager.cpp
    game/world/map_loading_settings.cpp
    game/world/map_snapshot_serializer.cpp
    # Scene
    game/scene/game_scene.cpp
    game/scene/appearance_customize_types.cpp
    game/scene/appearance_customize_scene.cpp
    game/scene/game_scene_battle_settlement.cpp
    game/scene/game_scene_reward_feedback.cpp
    game/scene/title_scene.cpp
    game/scene/save_slot_select_scene.cpp
    game/scene/pause_menu_scene.cpp
    game/scene/rest_dialog_scene.cpp
    game/scene/quest_offer_scene.cpp
    game/scene/recruit_offer_scene.cpp
    game/scene/shop_menu_scene.cpp
    game/scene/battle_action_presentation_plan.cpp
    game/scene/battle_animation_director.cpp
    game/scene/battle_background.cpp
    game/scene/battle_damage_popup_controller.cpp
    game/scene/battle_enemy_hp_bar_controller.cpp
    game/scene/battle_scene_data_bindings.cpp
    game/scene/battle_scene_entry.cpp
    game/scene/battle_victory_flow_controller.cpp
    game/scene/battle_scene.cpp
    game/scene/inventory_menu_character_panel.cpp
    game/scene/inventory_menu_scene.cpp
)

target_sources(game PRIVATE
    game/script/script_event_bridge.cpp
    game/script/script_game_api.cpp
    game/script/tinyfarm_script_module.cpp
)

# Game Debug 面板（仅在启用 Debug UI 时编译）
if(ENABLE_DEBUG_UI)
    target_sources(game PRIVATE
        game/debug/battle_debug_panel.cpp
        game/debug/player_debug_panel.cpp
        game/debug/inventory_debug_panel.cpp
        game/debug/quest_debug_panel.cpp
        game/debug/shop_debug_panel.cpp
        game/debug/game_time_debug_panel.cpp
        game/debug/save_load_debug_panel.cpp
        game/debug/map_inspector_debug_panel.cpp
        game/debug/blueprint_inspector_debug_panel.cpp
        game/debug/scheduler_profiler.cpp
        game/debug/scheduler_debug_panel.cpp
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
