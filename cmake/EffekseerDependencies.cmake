# ============================================
# Effekseer 依赖管理模块
# ============================================
# 功能：管理 Effekseer Runtime（仅接入 C++ Runtime + OpenGL 渲染后端）

include(FetchContent)

# CMake 3.30+ 对 FetchContent_Populate 的兼容策略。
# 保留显式 Populate + add_subdirectory，以便继续使用 EXCLUDE_FROM_ALL。
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# URL 归档应使用解压时间，避免上游文件时间戳影响增量构建。
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(EFFEKSEER_VERSION "1.80.6")
set(EFFEKSEER_RELEASE_TAG "1806")
set(EFFEKSEER_ARCHIVE_URL
    "https://github.com/effekseer/Effekseer/releases/download/${EFFEKSEER_RELEASE_TAG}/EffekseerForCpp${EFFEKSEER_VERSION}.zip")
set(EFFEKSEER_ARCHIVE_SHA256
    "b0004a4961f549aa44031956196388aa07125aaa7e2a364670f03a3602727c70")

function(setup_effekseer_dependencies)
    if(TARGET Effekseer AND TARGET EffekseerRendererGL)
        return()
    endif()

    message(STATUS "正在处理依赖: Effekseer ${EFFEKSEER_VERSION}")
    message(STATUS "  → 获取官方 C++ Runtime 发布包（SHA-256 校验）")

    FetchContent_Declare(
        effekseer_runtime
        URL "${EFFEKSEER_ARCHIVE_URL}"
        URL_HASH "SHA256=${EFFEKSEER_ARCHIVE_SHA256}"
    )
    FetchContent_GetProperties(effekseer_runtime)
    if(NOT effekseer_runtime_POPULATED)
        FetchContent_Populate(effekseer_runtime)
    endif()

    # MSVC：Effekseer 默认用静态 CRT（/MT），与主工程及其他依赖默认的 /MD 混用
    # 会在链接时报 LNK2038 RuntimeLibrary 不匹配，强制其改用动态 CRT。
    if(MSVC)
        set(USE_MSVC_RUNTIME_LIBRARY_DLL ON)
    endif()

    # 仅在当前函数/子目录作用域覆盖上游选项，避免污染主工程的通用 CACHE 变量。
    set(BUILD_EXAMPLES OFF)
    set(BUILD_TEST OFF)
    set(BUILD_VIEWER OFF)
    set(BUILD_EDITOR OFF)
    set(BUILD_WITH_EASY_PROFILER OFF)
    set(NETWORK_ENABLED OFF)

    # 图形后端：仅保留 OpenGL
    set(BUILD_GL ON)
    set(BUILD_VULKAN OFF)
    set(BUILD_DX9 OFF)
    set(BUILD_DX11 OFF)
    set(BUILD_DX12 OFF)
    set(BUILD_METAL OFF)
    set(USE_OPENGLES2 OFF)
    set(USE_OPENGLES3 OFF)
    set(USE_OPENGL3 ON)

    # 音频仍由项目自身的 MiniAudio 管理。
    set(USE_OPENAL OFF)
    set(USE_XAUDIO2 OFF)
    set(USE_DSOUND OFF)
    set(USE_OSM OFF)

    if(APPLE)
        set(_saved_has_osx_deployment_target OFF)
        if(DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
            set(_saved_has_osx_deployment_target ON)
            set(_saved_osx_deployment_target "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        endif()
    endif()

    add_subdirectory(
        "${effekseer_runtime_SOURCE_DIR}"
        "${effekseer_runtime_BINARY_DIR}"
        EXCLUDE_FROM_ALL
    )

    if(APPLE)
        if(_saved_has_osx_deployment_target)
            set(CMAKE_OSX_DEPLOYMENT_TARGET "${_saved_osx_deployment_target}" CACHE STRING "Restore project deployment target" FORCE)
            set(CMAKE_OSX_DEPLOYMENT_TARGET "${_saved_osx_deployment_target}" PARENT_SCOPE)
        else()
            unset(CMAKE_OSX_DEPLOYMENT_TARGET CACHE)
            unset(CMAKE_OSX_DEPLOYMENT_TARGET)
            unset(CMAKE_OSX_DEPLOYMENT_TARGET PARENT_SCOPE)
        endif()
    endif()

    if(NOT TARGET Effekseer)
        message(FATAL_ERROR "Effekseer 目标创建失败")
    endif()
    if(NOT TARGET EffekseerRendererGL)
        message(FATAL_ERROR "EffekseerRendererGL 目标创建失败")
    endif()

    message(STATUS "  ✓ Effekseer ${EFFEKSEER_VERSION} Runtime 已接入（Effekseer + EffekseerRendererGL）")
endfunction()
