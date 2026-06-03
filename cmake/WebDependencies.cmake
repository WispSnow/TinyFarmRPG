# ============================================
# WebAssembly dependency setup
# ============================================

include(FetchContent)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

function(tf_web_add_sdl3_port)
    if(TARGET SDL3::SDL3)
        return()
    endif()

    add_library(tf_web_sdl3 INTERFACE)
    target_compile_options(tf_web_sdl3 INTERFACE -sUSE_SDL=3)
    target_link_options(tf_web_sdl3 INTERFACE -sUSE_SDL=3)
    add_library(SDL3::SDL3 ALIAS tf_web_sdl3)
    message(STATUS "  ✓ Web SDL3 uses Emscripten port (-sUSE_SDL=3)")
endfunction()

function(tf_web_configure_thread_stubs)
    set(THREADS_PREFER_PTHREAD_FLAG OFF CACHE BOOL "Web build does not use pthread link flags" FORCE)
    set(CMAKE_HAVE_LIBC_PTHREAD TRUE CACHE INTERNAL "Emscripten single-thread libc provides pthread stubs" FORCE)
    set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "Web build uses no extra thread libraries" FORCE)
    set(CMAKE_USE_PTHREADS_INIT TRUE CACHE BOOL "Web build exposes pthread-compatible stubs" FORCE)
    message(STATUS "  ✓ Web Threads::Threads uses Emscripten single-thread stubs")
endfunction()

function(tf_web_fetch_dependency DEP_NAME GIT_REPO GIT_TAG LOCAL_PATH)
    set(_local_source_dir "${CMAKE_SOURCE_DIR}/${LOCAL_PATH}")
    if(EXISTS "${_local_source_dir}/CMakeLists.txt")
        message(STATUS "  → Web dependency ${DEP_NAME}: local source ${_local_source_dir}")
        add_subdirectory("${_local_source_dir}" "${CMAKE_BINARY_DIR}/_deps/${DEP_NAME}-web-build" EXCLUDE_FROM_ALL)
        return()
    endif()

    message(STATUS "  → Web dependency ${DEP_NAME}: fetching ${GIT_REPO} (${GIT_TAG})")
    FetchContent_Declare(
        ${DEP_NAME}
        GIT_REPOSITORY "${GIT_REPO}"
        GIT_TAG "${GIT_TAG}"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(${DEP_NAME})
endfunction()

function(tf_web_add_lua)
    if(TARGET Lua::Lua)
        return()
    endif()

    set(_lua_root "${CMAKE_SOURCE_DIR}/external/lua-5.4.8")
    if(NOT EXISTS "${_lua_root}/lua.h" AND EXISTS "${_lua_root}/src/lua.h")
        set(_lua_root "${_lua_root}/src")
    endif()
    if(NOT EXISTS "${_lua_root}/lua.h")
        message(FATAL_ERROR "Lua source for Web build not found: ${_lua_root}")
    endif()

    file(GLOB _lua_sources CONFIGURE_DEPENDS "${_lua_root}/*.c")
    list(REMOVE_ITEM _lua_sources
        "${_lua_root}/lua.c"
        "${_lua_root}/luac.c"
        "${_lua_root}/onelua.c")

    add_library(lua_static STATIC ${_lua_sources})
    target_include_directories(lua_static PUBLIC "${_lua_root}")
    target_compile_features(lua_static PUBLIC c_std_99)
    add_library(Lua::Lua ALIAS lua_static)
    message(STATUS "  ✓ Web Lua::Lua uses local source ${_lua_root}")
endfunction()

function(tf_web_add_sol2)
    if(TARGET sol2::sol2)
        return()
    endif()

    set(_sol2_include "${CMAKE_SOURCE_DIR}/external/sol2-3.5.0/include")
    if(NOT EXISTS "${_sol2_include}/sol/sol.hpp")
        message(FATAL_ERROR "sol2 headers for Web build not found: ${_sol2_include}")
    endif()

    add_library(sol2_headers INTERFACE)
    target_include_directories(sol2_headers INTERFACE "${_sol2_include}")
    target_compile_definitions(sol2_headers INTERFACE SOL_NO_EXCEPTIONS=1)
    target_link_libraries(sol2_headers INTERFACE Lua::Lua)
    add_library(sol2::sol2 ALIAS sol2_headers)
    message(STATUS "  ✓ Web sol2::sol2 uses local headers ${_sol2_include}")
endfunction()

function(tf_web_setup_project_dependencies)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "tf_web_setup_project_dependencies requires Emscripten.")
    endif()

    message(STATUS "Configuring WebAssembly dependencies")
    tf_web_configure_thread_stubs()
    tf_web_add_sdl3_port()

    set(_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Web dependencies are static" FORCE)

    tf_web_fetch_dependency(
        glm
        "https://github.com/g-truc/glm.git"
        "1.0.1"
        "external/glm-1.0.1"
    )
    tf_web_fetch_dependency(
        json
        "https://github.com/nlohmann/json.git"
        "v3.12.0"
        "external/json-3.12.0"
    )
    if(TARGET nlohmann_json)
        target_compile_definitions(nlohmann_json INTERFACE JSON_NOEXCEPTION)
    endif()

    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "spdlog: disable examples" FORCE)
    set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "spdlog: disable header-only examples" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "spdlog: disable tests" FORCE)
    set(SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "spdlog: disable header-only tests" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "spdlog: disable benchmarks" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "spdlog: static for Web" FORCE)
    tf_web_fetch_dependency(
        spdlog
        "https://github.com/gabime/spdlog.git"
        "v1.15.3"
        "external/spdlog-1.15.3"
    )
    if(TARGET spdlog)
        target_compile_definitions(spdlog PUBLIC SPDLOG_NO_EXCEPTIONS)
    endif()

    tf_web_fetch_dependency(
        EnTT
        "https://github.com/skypjack/entt.git"
        "v3.16.0"
        "external/entt-3.16.0"
    )

    set(FT_DISABLE_ZLIB TRUE CACHE BOOL "FreeType Web: disable zlib" FORCE)
    set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "FreeType Web: disable bzip2" FORCE)
    set(FT_DISABLE_PNG TRUE CACHE BOOL "FreeType Web: disable png" FORCE)
    set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "FreeType Web: disable harfbuzz cycle" FORCE)
    set(FT_DISABLE_BROTLI TRUE CACHE BOOL "FreeType Web: disable brotli" FORCE)
    tf_web_fetch_dependency(
        Freetype
        "https://github.com/freetype/freetype.git"
        "VER-2-14-1"
        "external/freetype-VER-2-14-1"
    )
    if(TARGET freetype AND NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()

    set(HB_HAVE_CAIRO OFF CACHE BOOL "HarfBuzz Web: disable cairo" FORCE)
    set(HB_HAVE_FREETYPE ON CACHE BOOL "HarfBuzz Web: enable freetype interop" FORCE)
    set(HB_HAVE_GRAPHITE2 OFF CACHE BOOL "HarfBuzz Web: disable graphite2" FORCE)
    set(HB_HAVE_GLIB OFF CACHE BOOL "HarfBuzz Web: disable glib" FORCE)
    set(HB_HAVE_ICU OFF CACHE BOOL "HarfBuzz Web: disable icu" FORCE)
    set(HB_HAVE_GOBJECT OFF CACHE BOOL "HarfBuzz Web: disable gobject" FORCE)
    set(HB_HAVE_INTROSPECTION OFF CACHE BOOL "HarfBuzz Web: disable introspection" FORCE)
    set(HB_BUILD_UTILS OFF CACHE BOOL "HarfBuzz Web: disable utils" FORCE)
    set(HB_BUILD_SUBSET OFF CACHE BOOL "HarfBuzz Web: disable subset" FORCE)
    tf_web_fetch_dependency(
        harfbuzz
        "https://github.com/harfbuzz/harfbuzz.git"
        "12.1.0"
        "external/harfbuzz-12.1.0"
    )
    if(TARGET harfbuzz AND NOT TARGET HarfBuzz::HarfBuzz)
        add_library(HarfBuzz::HarfBuzz ALIAS harfbuzz)
    elseif(TARGET harfbuzz::harfbuzz AND NOT TARGET HarfBuzz::HarfBuzz)
        add_library(HarfBuzz::HarfBuzz ALIAS harfbuzz::harfbuzz)
    endif()

    tf_web_add_lua()
    tf_web_add_sol2()

    set(BUILD_SHARED_LIBS "${_saved_build_shared_libs}" CACHE BOOL "Restore shared libs setting" FORCE)
endfunction()
