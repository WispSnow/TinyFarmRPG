# ============================================
# 编译器配置模块
# ============================================
# 功能：设置C++标准、编译选项、字符编码等

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项配置函数
# 用法：setup_compiler_options(目标名称)
function(setup_compiler_options TARGET_NAME)
    if(MSVC)
        # Visual Studio: 启用所有警告 + UTF-8编码支持 + 并行编译
        # /bigobj：sol2 绑定与大型场景 TU 的 COFF 节数会超过默认上限（C1128）
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /utf-8 /MP /bigobj)
        # windows.h 会经第三方头（如 EffekseerRendererGL.h）间接进入本项目 TU：
        # NOMINMAX 禁用其 min/max 宏（否则 std::max 等直接编译失败，C2589）；
        # WIN32_LEAN_AND_MEAN 裁剪少用的 Windows 头，减少宏污染并加快编译。
        target_compile_definitions(${TARGET_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    elseif(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        # MinGW/Clang on Windows: 设置UTF-8编码
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic -finput-charset=utf-8 -fexec-charset=utf-8)
        target_compile_definitions(${TARGET_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    else()
        # Linux/macOS: 标准警告选项
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(ENABLE_TSAN)
        if(MSVC)
            message(FATAL_ERROR "ENABLE_TSAN=ON is not supported on MSVC.")
        endif()

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
           CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
           CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
            target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
            target_link_options(${TARGET_NAME} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        else()
            message(FATAL_ERROR "ENABLE_TSAN=ON requires GCC, Clang, or AppleClang.")
        endif()
    endif()
endfunction()
