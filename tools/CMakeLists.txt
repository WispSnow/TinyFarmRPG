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

add_executable(ui_tester
    ui_tester/main.cpp
    ui_tester/ui_test_scene.cpp
)

target_link_libraries(ui_tester
    PRIVATE
        engine
)

target_include_directories(ui_tester
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/ui_tester
)

setup_compiler_options(ui_tester)
setup_asset_copy(ui_tester)
setup_config_copy(ui_tester)
setup_windows_dll_copy(ui_tester)

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
