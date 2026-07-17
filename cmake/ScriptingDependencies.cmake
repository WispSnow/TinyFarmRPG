# ============================================
# 脚本依赖管理模块
# ============================================
# 功能：管理 Lua / Sol2（独立于通用依赖宏）

include(FetchContent)

# CMake 3.30+ 对 FetchContent_Populate 的兼容策略
# TODO: 当最低 CMake 版本提升后，迁移到 FetchContent_MakeAvailable 流程并移除该策略。
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

function(resolve_lua_source_root OUT_VAR)
    set(_local_root "${CMAKE_SOURCE_DIR}/external/lua-5.4.8")
    if(EXISTS "${_local_root}/src/lua.h" OR EXISTS "${_local_root}/lua.h")
        message(STATUS "  → 使用本地源码: ${_local_root}")
        set(${OUT_VAR} "${_local_root}" PARENT_SCOPE)
        return()
    endif()

    message(STATUS "  → 本地源码不存在，在线获取: https://github.com/lua/lua (v5.4.8)")
    FetchContent_Declare(
        lua_vanilla
        GIT_REPOSITORY "https://github.com/lua/lua.git"
        GIT_TAG "v5.4.8"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_GetProperties(lua_vanilla)
    if(NOT lua_vanilla_POPULATED)
        FetchContent_Populate(lua_vanilla)
    endif()

    set(${OUT_VAR} "${lua_vanilla_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(setup_lua_dependency)
    message(STATUS "正在处理依赖: Lua")

    if(TARGET Lua::Lua)
        message(STATUS "  ✓ 已存在目标 Lua::Lua")
        return()
    endif()

    if(TF_USE_SYSTEM_DEPS)
        find_package(Lua 5.4 QUIET)
        if(TARGET Lua::Lua)
            message(STATUS "  ✓ 找到本地安装的 Lua")
            if(DEFINED LUA_INCLUDE_DIR)
                message(STATUS "     头文件: ${LUA_INCLUDE_DIR}")
            endif()
            return()
        endif()
    endif()

    resolve_lua_source_root(_lua_root)
    set(_lua_src_dir "${_lua_root}/src")
    if(NOT EXISTS "${_lua_src_dir}/lua.h")
        set(_lua_src_dir "${_lua_root}")
    endif()
    if(NOT EXISTS "${_lua_src_dir}/lua.h")
        message(FATAL_ERROR "Lua 源码目录无效: ${_lua_src_dir}")
    endif()

    file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${_lua_src_dir}/*.c")
    list(REMOVE_ITEM LUA_SOURCES
        "${_lua_src_dir}/lua.c"
        "${_lua_src_dir}/luac.c"
        "${_lua_src_dir}/onelua.c")

    if(LUA_SOURCES STREQUAL "")
        message(FATAL_ERROR "Lua 源码扫描失败: ${_lua_src_dir}")
    endif()

    add_library(lua_static STATIC ${LUA_SOURCES})
    target_include_directories(lua_static PUBLIC "${_lua_src_dir}")
    target_compile_features(lua_static PUBLIC c_std_99)

    if(UNIX)
        target_link_libraries(lua_static PUBLIC m)
        if(NOT APPLE)
            target_link_libraries(lua_static PUBLIC dl)
        endif()
    endif()

    add_library(Lua::Lua ALIAS lua_static)
    message(STATUS "  ✓ 使用源码构建 Lua::Lua (sources=${_lua_src_dir})")
endfunction()

function(resolve_sol2_source_root OUT_VAR)
    set(_local_root "${CMAKE_SOURCE_DIR}/external/sol2-3.5.0")
    if(EXISTS "${_local_root}/include/sol/sol.hpp")
        message(STATUS "  → 使用本地源码: ${_local_root}")
        set(${OUT_VAR} "${_local_root}" PARENT_SCOPE)
        return()
    endif()

    message(STATUS "  → 本地源码不存在，在线获取: https://github.com/ThePhD/sol2 (v3.5.0)")
    FetchContent_Declare(
        sol2_src
        GIT_REPOSITORY "https://github.com/ThePhD/sol2.git"
        GIT_TAG "v3.5.0"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_GetProperties(sol2_src)
    if(NOT sol2_src_POPULATED)
        FetchContent_Populate(sol2_src)
    endif()

    set(${OUT_VAR} "${sol2_src_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(setup_sol2_dependency)
    message(STATUS "正在处理依赖: sol2")

    if(TARGET sol2::sol2)
        message(STATUS "  ✓ 已存在目标 sol2::sol2")
        return()
    endif()

    if(TF_USE_SYSTEM_DEPS)
        find_package(sol2 QUIET CONFIG)
        if(TARGET sol2 AND NOT TARGET sol2::sol2)
            add_library(sol2::sol2 ALIAS sol2)
        endif()
        if(TARGET sol2::sol2)
            message(STATUS "  ✓ 找到本地安装的 sol2")
            return()
        endif()
    endif()

    resolve_sol2_source_root(_sol2_root)
    set(_sol2_include "${_sol2_root}/include")
    if(NOT EXISTS "${_sol2_include}/sol/sol.hpp")
        message(FATAL_ERROR "sol2 头文件目录无效: ${_sol2_include}")
    endif()

    add_library(sol2_headers INTERFACE)
    target_include_directories(sol2_headers INTERFACE "${_sol2_include}")
    target_link_libraries(sol2_headers INTERFACE Lua::Lua)
    add_library(sol2::sol2 ALIAS sol2_headers)

    message(STATUS "  ✓ 使用手动 INTERFACE 方式接入 sol2: ${_sol2_include}")
    message(STATUS "     说明: sol2 tag v3.5.0 (project version 4.0.0)")
endfunction()

function(setup_scripting_dependencies)
    setup_lua_dependency()
    setup_sol2_dependency()
endfunction()
