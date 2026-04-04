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
option(ENABLE_TSAN "启用 ThreadSanitizer（支持 Clang/GCC/AppleClang，且通常用于 Debug）" OFF)

# EnTT 资源/哈希 ID 使用 64 位，降低冲突风险
add_compile_definitions(ENTT_ID_TYPE=std::uint64_t)

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

# 调用依赖配置函数（定义在Dependencies.cmake中）
setup_project_dependencies()
setup_scripting_dependencies()
setup_effekseer_dependencies()
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

target_link_libraries(engine
    PUBLIC
        Effekseer
        EffekseerRendererGL
)
target_include_directories(engine
    PUBLIC
        ${EFFEKSEER_RUNTIME_INCLUDE_DIRS}
)

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

if(BUILD_TOOLS AND ENABLE_DEBUG_UI)
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
