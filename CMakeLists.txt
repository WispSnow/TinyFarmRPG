# ============================================
# 项目主配置文件
# ============================================
cmake_minimum_required(VERSION 3.13.0)
project(TinyFarmRPG VERSION 0.1.0 LANGUAGES C CXX)

# ============================================
# 基础配置
# ============================================

option(TF_BUILD_WEB "构建 WebAssembly/WebGL2 gameplay target" OFF)
option(TF_BUILD_WEB_SKELETON "构建第一轮 Web walking skeleton smoke target" OFF)
option(TF_WEB_ENABLE_PTHREADS "构建 Web pthreads 变体（需要 SharedArrayBuffer/COOP/COEP 发布环境）" OFF)

if(TF_BUILD_WEB AND NOT EMSCRIPTEN)
    message(FATAL_ERROR "TF_BUILD_WEB=ON must be configured with emcmake.")
endif()

if(TF_WEB_ENABLE_PTHREADS AND NOT TF_BUILD_WEB)
    message(FATAL_ERROR "TF_WEB_ENABLE_PTHREADS=ON requires TF_BUILD_WEB=ON.")
endif()

if(TF_BUILD_WEB)
    set(TF_DEFAULT_ENABLE_DEBUG_UI OFF)
    set(TF_DEFAULT_ENABLE_RMLUI_DEBUGGER OFF)
    set(TF_DEFAULT_ENABLE_EFFEKSEER OFF)
    set(TF_DEFAULT_ENABLE_TSAN OFF)
    set(TF_DEFAULT_BUILD_TOOLS OFF)
    set(TF_DEFAULT_BUILD_RMLUI_TESTER OFF)
    set(TF_DEFAULT_BUILD_LEARN OFF)
    set(TF_DEFAULT_BUILD_TESTING OFF)
    set(TF_DEFAULT_ENABLE_RUNTIME_THREADS ${TF_WEB_ENABLE_PTHREADS})
    set(TF_DEFAULT_WEB_DIRECT_MAP_BOOT OFF)
    set(TF_DEFAULT_WEB_ENABLE_RUNTIME_PACKAGES ON)
else()
    set(TF_DEFAULT_ENABLE_DEBUG_UI ON)
    set(TF_DEFAULT_ENABLE_RMLUI_DEBUGGER ON)
    set(TF_DEFAULT_ENABLE_EFFEKSEER ON)
    set(TF_DEFAULT_ENABLE_TSAN OFF)
    set(TF_DEFAULT_BUILD_TOOLS ON)
    set(TF_DEFAULT_BUILD_RMLUI_TESTER ON)
    set(TF_DEFAULT_BUILD_LEARN ON)
    set(TF_DEFAULT_BUILD_TESTING ON)
    set(TF_DEFAULT_ENABLE_RUNTIME_THREADS ON)
    set(TF_DEFAULT_WEB_DIRECT_MAP_BOOT OFF)
    set(TF_DEFAULT_WEB_ENABLE_RUNTIME_PACKAGES OFF)
endif()

if(NOT EMSCRIPTEN)
    # 指定搜索路径（用于find_package在编译期查找库）
    set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/prebuilt)
    list(APPEND CMAKE_PREFIX_PATH $ENV{HOME}/.local)
endif()

# 指定目标名称
set(TARGET ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME})
if(TF_BUILD_WEB)
    set(TARGET ${PROJECT_NAME}-Web)
endif()

# 依赖库默认链接类型：ON = 动态链接(.dll/.so/.dylib)，OFF = 静态链接(.lib/.a)
# 注意：可以在Dependencies.cmake中为每个库单独指定
option(BUILD_SHARED_LIBS "依赖库默认编译为动态库" OFF)
option(ENABLE_DEBUG_UI "启用 ImGui 调试面板" ${TF_DEFAULT_ENABLE_DEBUG_UI})
option(ENABLE_RMLUI_DEBUGGER "启用 RmlUi Debugger" ${TF_DEFAULT_ENABLE_RMLUI_DEBUGGER})
option(ENABLE_EFFEKSEER "启用 Effekseer VFX 后端" ${TF_DEFAULT_ENABLE_EFFEKSEER})
option(ENABLE_TSAN "启用 ThreadSanitizer（支持 Clang/GCC/AppleClang，且通常用于 Debug）" ${TF_DEFAULT_ENABLE_TSAN})
option(BUILD_TOOLS "是否构建工具" ${TF_DEFAULT_BUILD_TOOLS})
option(BUILD_RMLUI_TESTER "是否构建 rmlui_tester 工具" ${TF_DEFAULT_BUILD_RMLUI_TESTER})
option(BUILD_LEARN "构建学习/实验目标" ${TF_DEFAULT_BUILD_LEARN})
option(BUILD_TESTING "是否构建测试" ${TF_DEFAULT_BUILD_TESTING})
option(TF_ENABLE_RUNTIME_THREADS "启用运行时 worker 线程（Web 默认关闭，pthreads 变体再打开）" ${TF_DEFAULT_ENABLE_RUNTIME_THREADS})
option(TF_WEB_DIRECT_MAP_BOOT "Web gameplay target 直进 home_exterior，用于 Phase 10 bring-up" ${TF_DEFAULT_WEB_DIRECT_MAP_BOOT})
option(TF_WEB_ENABLE_RUNTIME_PACKAGES "启用 Web 运行时资源分包加载机制" ${TF_DEFAULT_WEB_ENABLE_RUNTIME_PACKAGES})

if(TF_WEB_ENABLE_PTHREADS)
    set(TF_ENABLE_RUNTIME_THREADS ON CACHE BOOL "启用运行时 worker 线程（Web 默认关闭，pthreads 变体再打开）" FORCE)
endif()

if(TF_WEB_DIRECT_MAP_BOOT AND NOT TF_BUILD_WEB)
    message(FATAL_ERROR "TF_WEB_DIRECT_MAP_BOOT=ON requires TF_BUILD_WEB=ON.")
endif()

if(TF_WEB_ENABLE_RUNTIME_PACKAGES AND NOT TF_BUILD_WEB)
    message(FATAL_ERROR "TF_WEB_ENABLE_RUNTIME_PACKAGES=ON requires TF_BUILD_WEB=ON.")
endif()

if(TF_BUILD_WEB AND TF_ENABLE_RUNTIME_THREADS AND NOT TF_WEB_ENABLE_PTHREADS)
    message(FATAL_ERROR "Web runtime threads require TF_WEB_ENABLE_PTHREADS=ON.")
endif()

# EnTT 资源/哈希 ID 使用 64 位；并行 ECS worker 会跨线程访问 EnTT storage，
# 因此启用 EnTT 文档建议的 atomic 内部状态。
add_compile_definitions(
    ENTT_ID_TYPE=std::uint64_t
    ENTT_USE_ATOMIC
)

if(TF_BUILD_WEB)
    add_compile_definitions(TF_BUILD_WEB)
endif()
if(TF_WEB_DIRECT_MAP_BOOT)
    add_compile_definitions(TF_WEB_DIRECT_MAP_BOOT)
endif()
if(TF_WEB_ENABLE_RUNTIME_PACKAGES)
    add_compile_definitions(TF_WEB_ENABLE_RUNTIME_PACKAGES)
endif()
if(TF_ENABLE_RUNTIME_THREADS)
    add_compile_definitions(TF_ENABLE_RUNTIME_THREADS)
endif()
if(TF_WEB_ENABLE_PTHREADS)
    add_compile_definitions(TF_WEB_ENABLE_PTHREADS)
endif()

# ============================================
# 引入模块化配置
# ============================================

# 编译器设置（C++标准、编译选项等）
include(cmake/CompilerSettings.cmake)

if(TF_BUILD_WEB AND TF_BUILD_WEB_SKELETON)
    include(cmake/WebPreload.cmake)
    add_subdirectory(src/web)
    message(STATUS "TinyFarmRPG Web skeleton target configured: ${TARGET}")
    return()
endif()

if(TF_BUILD_WEB)
    include(cmake/WebRuntime.cmake)
    include(cmake/WebDependencies.cmake)
    include(cmake/RmlUiDependencies.cmake)
else()
    # 运行时路径配置（RPATH）
    include(cmake/RuntimePath.cmake)

    # 依赖管理
    include(cmake/Dependencies.cmake)

    # 脚本依赖管理（Lua / Sol2）
    include(cmake/ScriptingDependencies.cmake)

    # Effekseer 依赖管理（可选）
    include(cmake/EffekseerDependencies.cmake)

    # RmlUi 依赖管理（可选）
    include(cmake/RmlUiDependencies.cmake)

    # ImGui配置
    include(cmake/ImGui.cmake)

    # OpenGL配置
    include(cmake/OpenGL.cmake)
endif()

# 构建辅助函数
include(cmake/BuildHelpers.cmake)

# 项目信息打印
include(cmake/ProjectInfo.cmake)

# 测试框架
include(CTest)

# ============================================
# 设置项目依赖
# ============================================

if(TF_BUILD_WEB)
    tf_web_setup_project_dependencies()
else()
    # 调用依赖配置函数（定义在Dependencies.cmake中）
    setup_project_dependencies()
    setup_scripting_dependencies()
    if(ENABLE_EFFEKSEER)
        setup_effekseer_dependencies()
    endif()
endif()
setup_rmlui_dependencies()

# ============================================
# 构建目标
# ============================================

# 创建 Engine 静态库
add_library(engine STATIC ${IMGUI_SOURCES})

if(ENABLE_DEBUG_UI)
    target_compile_definitions(engine PUBLIC TF_ENABLE_DEBUG_UI)
endif()
if(ENABLE_RMLUI_DEBUGGER)
    target_compile_definitions(engine PUBLIC TF_ENABLE_RMLUI_DEBUGGER)
endif()
if(ENABLE_EFFEKSEER)
    target_compile_definitions(engine PUBLIC TF_ENABLE_EFFEKSEER)
endif()

# 链接所有依赖库
target_link_libraries(engine
    PUBLIC
        SDL3::SDL3
        glm::glm
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        EnTT::EnTT
        Freetype::Freetype
        HarfBuzz::HarfBuzz
)

if(NOT TF_BUILD_WEB)
    target_link_libraries(engine
        PUBLIC
            SDL3_image::SDL3_image
            OpenGL::GL
            glad
    )
endif()

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/external/miniaudio
)

# 创建 Game 静态库
add_library(game STATIC)

target_link_libraries(game
    PUBLIC
        engine
)

target_link_libraries(engine
    PUBLIC
        Lua::Lua
        sol2::sol2
)

if(ENABLE_EFFEKSEER)
    target_link_libraries(engine
        PUBLIC
            Effekseer
            EffekseerRendererGL
    )
    target_include_directories(engine
        PUBLIC
            ${EFFEKSEER_RUNTIME_INCLUDE_DIRS}
    )
endif()

target_link_libraries(engine
    PUBLIC
        rmlui_core
)

if(ENABLE_RMLUI_DEBUGGER)
    target_link_libraries(engine
        PUBLIC
            rmlui_debugger
    )
endif()

# 创建可执行文件
add_executable(${TARGET})

target_link_libraries(${TARGET}
    PRIVATE
        game
)

target_include_directories(${TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 源文件填充（定义在 src/CMakeLists.txt 中）
add_subdirectory(src)

# ============================================
# 应用配置
# ============================================

# 设置编译选项（定义在CompilerSettings.cmake中）
setup_compiler_options(engine)
setup_compiler_options(game)
setup_compiler_options(${TARGET})

if(TF_BUILD_WEB)
    tf_apply_web_compile_options(engine)
    tf_apply_web_compile_options(game)
    tf_configure_web_executable(${TARGET})
else()
    # 配置资源文件复制（定义在BuildHelpers.cmake中）
    setup_asset_copy(${TARGET})

    # 配置 UI 资源复制（定义在BuildHelpers.cmake中）
    setup_ui_copy(${TARGET})

    # 配置脚本文件复制（定义在BuildHelpers.cmake中）
    setup_script_copy(${TARGET})

    # 配置配置文件复制（定义在BuildHelpers.cmake中）
    setup_config_copy(${TARGET})

    # 配置Windows DLL复制（定义在BuildHelpers.cmake中）
    setup_windows_dll_copy(${TARGET})
endif()

# ============================================
# 工具配置
# ============================================

if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# ============================================
# 学习/实验配置
# ============================================

if(BUILD_LEARN)
    add_subdirectory(learn)
endif()

# ============================================
# 测试配置
# ============================================

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================
# 打印配置信息
# ============================================

# 打印项目配置完成信息（定义在ProjectInfo.cmake中）
print_project_info(${TARGET})
