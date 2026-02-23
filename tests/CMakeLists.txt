# GoogleTest registration policy
#
# ASAN + gtest_discover_tests 在 macOS/libc++ 上可能在“列举测试列表”阶段触发
# gtest 内部容器检测误报并导致构建中断。ASAN 构建下改用源码扫描注册，避免在 build
# 期运行测试可执行文件；非 ASAN 构建继续使用 discover。
set(TF_ASAN_ENABLED OFF)
foreach(_flag_var CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS)
    if("${${_flag_var}}" MATCHES "(^|[ ;])-fsanitize=address($|[ ;])")
        set(TF_ASAN_ENABLED ON)
    endif()
endforeach()

include(GoogleTest)

# Engine unit tests

set(TEST_SOURCES
    engine/async/thread_pool_test.cpp
    engine/async/main_thread_command_queue_budget_test.cpp
    engine/core/time_test.cpp
    engine/core/config_factory_api_test.cpp
    engine/core/config_loop_timing_test.cpp
    engine/core/game_app_no_exceptions_test.cpp
    engine/core/game_app_dispatcher_trace_test.cpp
    engine/core/ecs_safety_test.cpp
    engine/audio/audio_player_test.cpp
    engine/audio/audio_player_no_exceptions_test.cpp
    engine/input/input_manager_test.cpp
    engine/input/input_manager_no_exceptions_test.cpp
    engine/scene/entry_to_first_frame_safety_test.cpp
    engine/scene/scene_manager_safety_test.cpp
    engine/scene/render_interpolation_pipeline_test.cpp
    engine/factory_visibility_test.cpp
    engine/resource/font_manager_create_test.cpp
    engine/resource/font_preprocess_service_test.cpp
    engine/resource/image_decode_service_test.cpp
    engine/resource/resource_manager_no_exceptions_test.cpp
    engine/resource/resource_manager_texture_handle_api_test.cpp
    engine/resource/resource_manager_audio_handle_api_test.cpp
    engine/resource/resource_manager_strict_preload_api_test.cpp
    engine/resource/resource_manager_font_event_dispatch_test.cpp
    engine/resource/resource_manager_submanager_visibility_test.cpp
    engine/resource/resource_manager_clear_orchestration_test.cpp
    engine/ui/ui_button_factory_api_test.cpp
    engine/loader/level_loader_layer_name_test.cpp
    engine/loader/level_preprocess_service_test.cpp
    engine/loader/level_loader_async_stage_test.cpp
    engine/loader/level_loader_no_exceptions_test.cpp
    engine/loader/level_loader_no_json_value_test.cpp
    engine/loader/level_loader_tile_flag_warning_test.cpp
    engine/loader/basic_entity_builder_no_json_value_test.cpp
    engine/system/render_system_view_use_test.cpp
    engine/system/remove_entity_system_test.cpp
    engine/system/movement_system_dynamic_grid_sync_test.cpp
    engine/system/parallel_wave_scheduler_test.cpp
    engine/render/nine_slice_test.cpp
    engine/render/text_renderer_no_exceptions_test.cpp
    engine/render/render_context_no_exceptions_test.cpp
    engine/render/sprite_batch_source_test.cpp
    engine/ui/ui_preset_manager_sound_test.cpp
    engine/ui/ui_preset_manager_no_exceptions_test.cpp
    engine/ui/ui_preset_manager_invalid_input_test.cpp
    engine/ui/ui_preset_manager_debug_api_test.cpp
    engine/ui/ui_interaction_state_source_test.cpp
    engine/ui/ui_layout_source_test.cpp
    engine/ui/ui_interaction_runtime_test.cpp
    engine/ui/ui_layout_invalidation_test.cpp
    engine/ui/ui_stack_layout_test.cpp
    engine/ui/ui_grid_layout_test.cpp
    engine/spatial/static_tile_grid_sweep_test.cpp
    engine/spatial/static_tile_grid_rect_query_test.cpp
    engine/spatial/dynamic_entity_grid_test.cpp
    engine/spatial/spatial_index_manager_raii_test.cpp
    engine/spatial/spatial_index_manager_sweep_result_test.cpp
    engine/resource/resource_manager_filesystem_no_exceptions_test.cpp
)

if(ENABLE_DEBUG_UI)
    list(APPEND TEST_SOURCES
        engine/debug/dispatcher_trace_test.cpp
    )
endif()

add_executable(engine_tests ${TEST_SOURCES})

target_link_libraries(engine_tests
    PRIVATE
        engine
        GTest::gtest_main
        GTest::gmock_main
)

target_compile_definitions(engine_tests
    PRIVATE
        PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

setup_compiler_options(engine_tests)

if(TF_ASAN_ENABLED)
    gtest_add_tests(TARGET engine_tests TEST_LIST ENGINE_TEST_LIST)
else()
    gtest_discover_tests(engine_tests)
endif()

# Game unit tests

set(GAME_TEST_SOURCES
    game/blueprint_manager_smoke_test.cpp
    game/action_sound_system_test.cpp
    game/farm_system_hoe_blocking_test.cpp
    game/farm_system_harvest_inventory_full_test.cpp
    game/command_event_flow_test.cpp
    game/inventory_domain_service_test.cpp
    game/inventory_hotbar_consistency_test.cpp
    game/inventory_move_hotbar_follow_test.cpp
    game/interaction_command_pipeline_test.cpp
    game/player_control_system_targeting_test.cpp
    game/state_system_dirty_tag_test.cpp
    game/save_data_v3_roundtrip_test.cpp
    game/save_migrator_test.cpp
    game/save_service_async_test.cpp
    game/save_slot_summary_test.cpp
    game/light_toggle_system_test.cpp
    game/game_scene_light_toggle_hook_test.cpp
    game/game_scene_runtime_assembly_test.cpp
    game/system_scheduler_profile_test.cpp
    game/system_scheduler_transition_gate_test.cpp
    game/system_scheduler_invariant_test.cpp
    game/title_scene_menu_button_test.cpp
    game/pause_menu_scene_save_load_disabled_test.cpp
    game/pause_menu_scene_async_save_ui_test.cpp
    game/save_slot_select_scene_enable_state_test.cpp
    game/time_advance_sleep_test.cpp
    game/rest_area_interaction_test.cpp
    game/time_of_day_light_system_test.cpp
    game/item_use_system_test.cpp
    game/npc_animal_parallel_domain_test.cpp
    game/map_loading_settings_test.cpp
    game/map_manager_async_preload_test.cpp
    game/world/async_preload_pipeline_test.cpp
    game/world_state_test.cpp
    game/ui_layout_integration_test.cpp
)

if(ENABLE_SCRIPTING)
    list(APPEND GAME_TEST_SOURCES
        game/script_host_smoke_test.cpp
        game/script_host_command_bridge_test.cpp
        game/script_host_security_boundary_test.cpp
        game/script_host_handle_lifecycle_test.cpp
    )
endif()

if(ENABLE_DEBUG_UI)
    list(APPEND GAME_TEST_SOURCES
        game/system_scheduler_profiler_test.cpp
    )
endif()

add_executable(game_tests ${GAME_TEST_SOURCES})

target_link_libraries(game_tests
    PRIVATE
        game
        GTest::gtest_main
        GTest::gmock_main
)

target_compile_definitions(game_tests
    PRIVATE
        PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

setup_compiler_options(game_tests)

if(TF_ASAN_ENABLED)
    gtest_add_tests(TARGET game_tests TEST_LIST GAME_TEST_LIST)
else()
    gtest_discover_tests(game_tests)
endif()
