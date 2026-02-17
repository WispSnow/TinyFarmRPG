# Engine unit tests

set(TEST_SOURCES
    engine/core/time_test.cpp
    engine/core/config_factory_api_test.cpp
    engine/core/game_app_no_exceptions_test.cpp
    engine/core/game_app_dispatcher_trace_test.cpp
    engine/core/ecs_safety_test.cpp
    engine/audio/audio_player_test.cpp
    engine/audio/audio_player_no_exceptions_test.cpp
    engine/input/input_manager_test.cpp
    engine/input/input_manager_no_exceptions_test.cpp
    engine/scene/entry_to_first_frame_safety_test.cpp
    engine/scene/scene_manager_safety_test.cpp
    engine/factory_visibility_test.cpp
    engine/resource/font_manager_create_test.cpp
    engine/resource/resource_manager_no_exceptions_test.cpp
    engine/ui/ui_button_factory_api_test.cpp
    engine/loader/level_loader_layer_name_test.cpp
    engine/loader/level_loader_no_exceptions_test.cpp
    engine/loader/level_loader_no_json_value_test.cpp
    engine/loader/level_loader_tile_flag_warning_test.cpp
    engine/loader/basic_entity_builder_no_json_value_test.cpp
    engine/system/render_system_view_use_test.cpp
    engine/system/remove_entity_system_test.cpp
    engine/system/movement_system_dynamic_grid_sync_test.cpp
    engine/render/nine_slice_test.cpp
    engine/render/text_renderer_no_exceptions_test.cpp
    engine/render/render_context_no_exceptions_test.cpp
    engine/render/sprite_batch_source_test.cpp
    engine/ui/ui_preset_manager_sound_test.cpp
    engine/ui/ui_preset_manager_no_exceptions_test.cpp
    engine/ui/ui_preset_manager_invalid_input_test.cpp
    engine/ui/ui_preset_manager_debug_api_test.cpp
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

include(GoogleTest)
gtest_discover_tests(engine_tests)

# Game unit tests

set(GAME_TEST_SOURCES
    game/blueprint_manager_smoke_test.cpp
    game/action_sound_system_test.cpp
    game/farm_system_hoe_blocking_test.cpp
    game/farm_system_harvest_inventory_full_test.cpp
    game/command_event_flow_test.cpp
    game/inventory_hotbar_consistency_test.cpp
    game/interaction_command_pipeline_test.cpp
    game/player_control_system_targeting_test.cpp
    game/state_system_dirty_tag_test.cpp
    game/save_slot_summary_test.cpp
    game/light_toggle_system_test.cpp
    game/game_scene_light_toggle_hook_test.cpp
    game/game_scene_runtime_assembly_test.cpp
    game/system_scheduler_profile_test.cpp
    game/system_scheduler_transition_gate_test.cpp
    game/system_scheduler_invariant_test.cpp
    game/title_scene_menu_button_test.cpp
    game/pause_menu_scene_save_load_disabled_test.cpp
    game/time_advance_sleep_test.cpp
    game/rest_area_interaction_test.cpp
    game/time_of_day_light_system_test.cpp
    game/item_use_system_test.cpp
    game/map_loading_settings_test.cpp
    game/world_state_test.cpp
)

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

gtest_discover_tests(game_tests)
