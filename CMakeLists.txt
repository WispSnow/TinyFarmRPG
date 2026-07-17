# ============================================
# 项目主配置文件
# ============================================
cmake_minimum_required(VERSION 3.13.0)
project(TinyFarmRPG VERSION 0.1.0 LANGUAGES C CXX)

# ============================================
# 基础配置
# ============================================

# 指定搜索路径（用于find_package在编译期查找库）
set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/prebuilt)
list(APPEND CMAKE_PREFIX_PATH $ENV{HOME}/.local)

# 指定目标名称
set(TARGET ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME})

# 依赖库默认链接类型：ON = 动态链接(.dll/.so/.dylib)，OFF = 静态链接(.lib/.a)
# 注意：可以在Dependencies.cmake中为每个库单独指定
option(BUILD_SHARED_LIBS "依赖库默认编译为动态库" OFF)
option(ENABLE_DEBUG_UI "启用 ImGui 调试面板" ON)
option(ENABLE_RMLUI_DEBUGGER "启用 RmlUi Debugger" ON)
option(ENABLE_EFFEKSEER "启用 Effekseer VFX 后端" ON)
option(ENABLE_TSAN "启用 ThreadSanitizer（支持 Clang/GCC/AppleClang，且通常用于 Debug）" OFF)

# EnTT 资源/哈希 ID 使用 64 位；并行 ECS worker 会跨线程访问 EnTT storage，
# 因此启用 EnTT 文档建议的 atomic 内部状态。
add_compile_definitions(
    ENTT_ID_TYPE=std::uint64_t
    ENTT_USE_ATOMIC
)

# ============================================
# 引入模块化配置
# ============================================

# 编译器设置（C++标准、编译选项等）
include(cmake/CompilerSettings.cmake)

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

# 构建辅助函数
include(cmake/BuildHelpers.cmake)

# 项目信息打印
include(cmake/ProjectInfo.cmake)

# 测试框架
include(CTest)

# ============================================
# 设置项目依赖
# ============================================

# 线程库（engine 的 std::jthread / std::thread 依赖；Linux 上需显式链接 pthread）
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 调用依赖配置函数（定义在Dependencies.cmake中）
setup_project_dependencies()
setup_scripting_dependencies()
if(ENABLE_EFFEKSEER)
    setup_effekseer_dependencies()
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
        SDL3_image::SDL3_image
        glm::glm
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        EnTT::EnTT
        OpenGL::GL
        glad
        Freetype::Freetype
        HarfBuzz::HarfBuzz
        Threads::Threads
)

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
# WIN32：Windows 上标记为 GUI 子系统（不弹控制台），入口由 SDL_main.h 提供的
# WinMain 承接；非 Windows 平台忽略该标志。控制台工具与测试不加此标志，
# 否则它们的 stdout 在 Windows 交互终端中不可见。
add_executable(${TARGET} WIN32)

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

# ============================================
# 工具配置
# ============================================

option(BUILD_TOOLS "是否构建工具" ON)
option(BUILD_RMLUI_TESTER "是否构建 rmlui_tester 工具" ON)

if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# ============================================
# 学习/实验配置
# ============================================

option(BUILD_LEARN "构建学习/实验目标" ON)

if(BUILD_LEARN)
    add_subdirectory(learn)
endif()

# ============================================
# 测试配置
# ============================================

option(BUILD_TESTING "是否构建测试" ON)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================
# 打印配置信息
# ============================================

# 打印项目配置完成信息（定义在ProjectInfo.cmake中）
print_project_info(${TARGET})
