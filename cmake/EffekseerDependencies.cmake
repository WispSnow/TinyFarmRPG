# ============================================
# Effekseer 依赖管理模块
# ============================================
# 功能：管理 Effekseer Runtime（仅接入 C++ Runtime + OpenGL 渲染后端）

function(setup_effekseer_dependencies)
    if(TARGET Effekseer AND TARGET EffekseerRendererGL)
        set(_includes
            "${CMAKE_SOURCE_DIR}/external/Effekseer-1.7.3.0/Dev/Cpp/Effekseer"
            "${CMAKE_SOURCE_DIR}/external/Effekseer-1.7.3.0/Dev/Cpp/EffekseerRendererGL")
        set(EFFEKSEER_RUNTIME_INCLUDE_DIRS "${_includes}" PARENT_SCOPE)
        return()
    endif()

    set(EFFEKSEER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/Effekseer-1.7.3.0")
    if(NOT EXISTS "${EFFEKSEER_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "Effekseer 源码目录不存在: ${EFFEKSEER_SOURCE_DIR}")
    endif()

    message(STATUS "正在处理依赖: Effekseer")
    message(STATUS "  → 使用本地源码: ${EFFEKSEER_SOURCE_DIR}")

    # MSVC：Effekseer 默认用静态 CRT（/MT），与主工程及其他依赖默认的 /MD 混用
    # 会在链接时报 LNK2038 RuntimeLibrary 不匹配，强制其改用动态 CRT。
    if(MSVC)
        set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "Effekseer: use dynamic CRT (/MD) to match the project" FORCE)
    endif()

    # 顶层模块开关（external/Effekseer-1.7.3.0/CMakeLists.txt）
    set(BUILD_VIEWER OFF CACHE BOOL "Effekseer: disable viewer" FORCE)
    set(BUILD_EDITOR OFF CACHE BOOL "Effekseer: disable editor" FORCE)
    set(BUILD_TEST OFF CACHE BOOL "Effekseer: disable tests" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Effekseer: disable examples" FORCE)
    set(BUILD_UNITYPLUGIN OFF CACHE BOOL "Effekseer: disable unity plugin" FORCE)
    set(BUILD_UNITYPLUGIN_FOR_IOS OFF CACHE BOOL "Effekseer: disable unity plugin ios" FORCE)
    set(BUILD_WITH_EASY_PROFILER OFF CACHE BOOL "Effekseer: disable profiler" FORCE)
    set(NETWORK_ENABLED OFF CACHE BOOL "Effekseer: disable network module" FORCE)

    # 图形后端：仅保留 OpenGL
    set(BUILD_GL ON CACHE BOOL "Effekseer: build OpenGL renderer" FORCE)
    set(BUILD_VULKAN OFF CACHE BOOL "Effekseer: disable Vulkan renderer" FORCE)
    set(BUILD_DX9 OFF CACHE BOOL "Effekseer: disable DX9 renderer" FORCE)
    set(BUILD_DX11 OFF CACHE BOOL "Effekseer: disable DX11 renderer" FORCE)
    set(BUILD_DX12 OFF CACHE BOOL "Effekseer: disable DX12 renderer" FORCE)
    set(BUILD_METAL OFF CACHE BOOL "Effekseer: disable Metal renderer" FORCE)

    # 音频后端：首版不接入
    set(USE_OPENAL OFF CACHE BOOL "Effekseer: disable OpenAL" FORCE)
    set(USE_XAUDIO2 OFF CACHE BOOL "Effekseer: disable XAudio2" FORCE)
    set(USE_DSOUND OFF CACHE BOOL "Effekseer: disable DirectSound" FORCE)
    set(USE_OSM OFF CACHE BOOL "Effekseer: disable OpenSoundMixer" FORCE)

    if(APPLE)
        set(_saved_has_osx_deployment_target OFF)
        if(DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
            set(_saved_has_osx_deployment_target ON)
            set(_saved_osx_deployment_target "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        endif()
    endif()

    add_subdirectory(
        "${EFFEKSEER_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/effekseer-build"
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

    set(_includes
        "${EFFEKSEER_SOURCE_DIR}/Dev/Cpp/Effekseer"
        "${EFFEKSEER_SOURCE_DIR}/Dev/Cpp/EffekseerRendererGL")
    set(EFFEKSEER_RUNTIME_INCLUDE_DIRS "${_includes}" PARENT_SCOPE)

    message(STATUS "  ✓ Effekseer Runtime 已接入（Effekseer + EffekseerRendererGL）")
endfunction()
