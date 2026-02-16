# ============================================
# OpenGL 配置模块
# ============================================
# 功能：配置OpenGL相关库和头文件路径

find_package(OpenGL REQUIRED)

# 添加Glad库和stb_image.h头文件路径
add_library(glad external/glad/src/glad.c)
target_include_directories(glad
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
)