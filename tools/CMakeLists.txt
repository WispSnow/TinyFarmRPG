# Developer tools targets

add_executable(visual_tester
    visual_tester/main.cpp
    visual_tester/visual_test_cases.cpp
    visual_tester/visual_test_suite_scene.cpp
)

target_link_libraries(visual_tester
    PRIVATE
        engine
)

target_include_directories(visual_tester
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/visual_tester
)

setup_compiler_options(visual_tester)
setup_asset_copy(visual_tester)
setup_config_copy(visual_tester)
setup_windows_dll_copy(visual_tester)

if(BUILD_RMLUI_TESTER)
    add_executable(rmlui_tester
        rmlui_tester/main.cpp
        rmlui_tester/rmlui_test_scene.cpp
    )

    target_link_libraries(rmlui_tester
        PRIVATE
            engine
    )

    target_include_directories(rmlui_tester
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/rmlui_tester
    )

    setup_compiler_options(rmlui_tester)
    setup_asset_copy(rmlui_tester)
    setup_ui_copy(rmlui_tester)
    setup_config_copy(rmlui_tester)
    setup_windows_dll_copy(rmlui_tester)
endif()

add_executable(battle_tester
    battle_tester/main.cpp
    battle_tester/battle_tester_scene.cpp
)

target_link_libraries(battle_tester
    PRIVATE
        game
)

target_include_directories(battle_tester
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/battle_tester
)

setup_compiler_options(battle_tester)
setup_asset_copy(battle_tester)
setup_ui_copy(battle_tester)
setup_script_copy(battle_tester)
setup_config_copy(battle_tester)
setup_windows_dll_copy(battle_tester)

add_executable(scheduler_dot_dump
    scheduler_dot_dump/main.cpp
)

target_link_libraries(scheduler_dot_dump
    PRIVATE
        game
)

target_include_directories(scheduler_dot_dump
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

setup_compiler_options(scheduler_dot_dump)
setup_windows_dll_copy(scheduler_dot_dump)
