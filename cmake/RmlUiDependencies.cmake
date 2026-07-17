# ============================================
# RmlUi 依赖管理模块
# ============================================
# 功能：管理 RmlUi Core + Debugger（不使用官方 SDL/GL3 backend 目标）

include(FetchContent)

# CMake 3.30+ 对 FetchContent_Populate 的兼容策略
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(RMLUI_LOCAL_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/RmlUi-6.2" CACHE PATH "本地 RmlUi 源码目录")
set(RMLUI_GIT_REPOSITORY "https://github.com/mikke89/RmlUi.git" CACHE STRING "RmlUi git 仓库地址")
set(RMLUI_GIT_TAG "6.2" CACHE STRING "RmlUi git 版本（tag/branch/commit）")

function(resolve_rmlui_source_root OUT_VAR)
    if(EXISTS "${RMLUI_LOCAL_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "  → 使用本地源码: ${RMLUI_LOCAL_SOURCE_DIR}")
        set(${OUT_VAR} "${RMLUI_LOCAL_SOURCE_DIR}" PARENT_SCOPE)
        return()
    endif()

    message(STATUS "  → 本地源码不存在，在线获取: ${RMLUI_GIT_REPOSITORY} (${RMLUI_GIT_TAG})")
    FetchContent_Declare(
        rmlui_src
        GIT_REPOSITORY "${RMLUI_GIT_REPOSITORY}"
        GIT_TAG "${RMLUI_GIT_TAG}"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_GetProperties(rmlui_src)
    if(NOT rmlui_src_POPULATED)
        FetchContent_Populate(rmlui_src)
    endif()

    set(${OUT_VAR} "${rmlui_src_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(setup_rmlui_dependencies)
    message(STATUS "正在处理依赖: RmlUi")

    if(TARGET rmlui_core AND TARGET rmlui_debugger)
        return()
    endif()

    resolve_rmlui_source_root(RMLUI_SOURCE_DIR)
    set(RMLUI_SOURCE_DIR_RESOLVED "${RMLUI_SOURCE_DIR}" CACHE PATH "RmlUi 解析后源码目录" FORCE)

    if(NOT EXISTS "${RMLUI_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "RmlUi 源码目录不存在: ${RMLUI_SOURCE_DIR}")
    endif()

    macro(_tf_save_var VAR)
        if(DEFINED ${VAR})
            set(_saved_${VAR}_defined TRUE)
            set(_saved_${VAR}_value "${${VAR}}")
        else()
            set(_saved_${VAR}_defined FALSE)
        endif()

        get_property(_saved_${VAR}_in_cache CACHE ${VAR} PROPERTY VALUE SET)
        if(_saved_${VAR}_in_cache)
            get_property(_saved_${VAR}_cache_type CACHE ${VAR} PROPERTY TYPE)
        endif()
    endmacro()

    macro(_tf_restore_var VAR)
        # 若变量原本不在 CACHE 中，先移除本模块写入的 CACHE 条目，避免污染主工程缓存。
        if(_saved_${VAR}_in_cache)
            if(_saved_${VAR}_defined)
                set(${VAR} "${_saved_${VAR}_value}" CACHE ${_saved_${VAR}_cache_type} "Restore ${VAR}" FORCE)
            else()
                unset(${VAR} CACHE)
                unset(${VAR})
            endif()
        else()
            unset(${VAR} CACHE)
            if(_saved_${VAR}_defined)
                set(${VAR} "${_saved_${VAR}_value}")
            else()
                unset(${VAR})
            endif()
        endif()
    endmacro()

    _tf_save_var(BUILD_SHARED_LIBS)
    _tf_save_var(RMLUI_FONT_ENGINE)
    _tf_save_var(RMLUI_SAMPLES)
    _tf_save_var(RMLUI_TESTS)
    _tf_save_var(RMLUI_LUA_BINDINGS)
    _tf_save_var(RMLUI_LOTTIE_PLUGIN)
    _tf_save_var(RMLUI_SVG_PLUGIN)
    _tf_save_var(RMLUI_COMPILER_OPTIONS)

    if(APPLE)
        _tf_save_var(CMAKE_OSX_DEPLOYMENT_TARGET)
    endif()

    # RmlUi 的 freetype 字体引擎要求存在 Freetype::Freetype 目标。
    # 从源码构建的 FreeType 只定义小写 freetype 目标（无命名空间别名），这里兜底补齐；
    # 若目标仍缺失，提前给出明确错误，避免 RmlUi 内部报出难定位的 find_package 失败。
    if(NOT TARGET Freetype::Freetype AND TARGET freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()
    if(NOT TARGET Freetype::Freetype)
        message(FATAL_ERROR "RmlUi 需要 Freetype::Freetype 目标：请确认 Dependencies.cmake 的 Freetype 步骤已成功（系统安装或 external/freetype 源码构建）。")
    endif()

    # 仅在本模块作用域中覆盖 RmlUi 选项
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "RmlUi: force static libs" FORCE)
    set(RMLUI_FONT_ENGINE freetype CACHE STRING "RmlUi: use freetype font engine" FORCE)
    set(RMLUI_SAMPLES OFF CACHE BOOL "RmlUi: disable samples" FORCE)
    set(RMLUI_TESTS OFF CACHE BOOL "RmlUi: disable tests" FORCE)
    set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "RmlUi: disable lua bindings" FORCE)
    set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "RmlUi: disable lottie plugin" FORCE)
    set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "RmlUi: disable svg plugin" FORCE)
    set(RMLUI_COMPILER_OPTIONS OFF CACHE BOOL "RmlUi: use project compiler options" FORCE)

    add_subdirectory(
        "${RMLUI_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/rmlui-build"
        EXCLUDE_FROM_ALL
    )

    _tf_restore_var(BUILD_SHARED_LIBS)
    _tf_restore_var(RMLUI_FONT_ENGINE)
    _tf_restore_var(RMLUI_SAMPLES)
    _tf_restore_var(RMLUI_TESTS)
    _tf_restore_var(RMLUI_LUA_BINDINGS)
    _tf_restore_var(RMLUI_LOTTIE_PLUGIN)
    _tf_restore_var(RMLUI_SVG_PLUGIN)
    _tf_restore_var(RMLUI_COMPILER_OPTIONS)

    if(APPLE)
        _tf_restore_var(CMAKE_OSX_DEPLOYMENT_TARGET)
        # 同步到父作用域，避免后续模块读取到覆盖值
        if(DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
            set(CMAKE_OSX_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}" PARENT_SCOPE)
        else()
            unset(CMAKE_OSX_DEPLOYMENT_TARGET PARENT_SCOPE)
        endif()
    endif()

    if(NOT TARGET rmlui_core)
        message(FATAL_ERROR "RmlUi 目标创建失败: rmlui_core")
    endif()
    if(NOT TARGET rmlui_debugger)
        message(FATAL_ERROR "RmlUi 目标创建失败: rmlui_debugger")
    endif()

    message(STATUS "  ✓ RmlUi 已接入（rmlui_core + rmlui_debugger）")
endfunction()
