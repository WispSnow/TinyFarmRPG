# ============================================
# 学习/实验目标
# ============================================

# add_learn_target(NAME [LIBS lib1 lib2 ...] [SOURCES file1.cpp file2.cpp ...])
#
# 创建 learn_<NAME> 可执行目标：
#   - 默认链接 engine，可通过 LIBS 追加额外库
#   - 默认收集 learn/<NAME>/*.cpp，可通过 SOURCES 覆盖
#   - 自动应用编译选项和资源复制
macro(add_learn_target NAME)
    cmake_parse_arguments(LEARN "" "" "LIBS;SOURCES" ${ARGN})

    set(_target learn_${NAME})

    if(LEARN_SOURCES)
        set(_sources ${LEARN_SOURCES})
    else()
        file(GLOB _sources ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/*.cpp)
    endif()

    add_executable(${_target} ${_sources})

    target_link_libraries(${_target}
        PRIVATE
            engine
            ${LEARN_LIBS}
    )

    target_include_directories(${_target}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}
    )

    setup_compiler_options(${_target})
    setup_asset_copy(${_target})
    setup_ui_copy(${_target})
    setup_config_copy(${_target})
    setup_windows_dll_copy(${_target})
endmacro()

# ============================================
# 实验目标列表
# ============================================

add_learn_target(rmlui_basics)
add_learn_target(rmlui_box_model)
add_learn_target(rmlui_flexbox)
add_learn_target(rmlui_styling)
add_learn_target(rmlui_events)
add_learn_target(rmlui_data_binding)
add_learn_target(rmlui_forms)
add_learn_target(rmlui_custom_elements)
add_learn_target(rmlui_spritesheet)
add_learn_target(rmlui_animation)
add_learn_target(rmlui_filters)
add_learn_target(jrpg_window)
add_learn_target(jrpg_dialogue)
