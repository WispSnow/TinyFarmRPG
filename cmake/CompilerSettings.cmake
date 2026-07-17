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
        # Visual Studio: 启用所有警告、UTF-8 编码和目标级并行编译。
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /utf-8 /MP)
    elseif(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        # MinGW/Clang on Windows: 设置UTF-8编码
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic -finput-charset=utf-8 -fexec-charset=utf-8)
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
