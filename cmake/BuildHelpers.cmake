# ============================================
# 构建辅助函数模块
# ============================================
# 功能：资源复制、DLL复制等构建辅助功能

function(_setup_runtime_directory_sync TARGET_NAME COPY_NAME SOURCE_DIR)
    set(options COPY_IMGUI_INI)
    cmake_parse_arguments(SYNC "${options}" "" "" ${ARGN})

    set(SYNC_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/scripts/SyncDirectory.cmake")
    set(SYNC_TARGET "${TARGET_NAME}_sync_${COPY_NAME}")
    set(TARGET_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    if(CMAKE_CONFIGURATION_TYPES)
        string(APPEND TARGET_BASE_DIR "/$<CONFIG>")
    endif()
    set(TARGET_DIR "${TARGET_BASE_DIR}/${COPY_NAME}")
    set(EXTRA_FILE_ARGS)
    if(SYNC_COPY_IMGUI_INI)
        list(APPEND EXTRA_FILE_ARGS
            "-DEXTRA_SOURCE_FILE=${CMAKE_SOURCE_DIR}/imgui.ini"
            "-DEXTRA_TARGET_FILE=${TARGET_BASE_DIR}/imgui.ini"
        )
    endif()

    # 自定义目标每次被请求时都会执行；脚本内部使用 copy_if_different 保持增量。
    # 这让 JSON/Lua/RML/纹理变更无需触发 C++ 重链接也能同步到运行目录。
    add_custom_target(${SYNC_TARGET}
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${SOURCE_DIR}"
            "-DTARGET_DIR=${TARGET_DIR}"
            ${EXTRA_FILE_ARGS}
            -P "${SYNC_SCRIPT}"
        COMMENT "Sync ${COPY_NAME} files for ${TARGET_NAME}"
        VERBATIM
    )
    add_dependencies(${TARGET_NAME} ${SYNC_TARGET})
endfunction()

# ============================================
# 配置资源文件复制
# 用法：setup_asset_copy(目标名称)
# ============================================
function(setup_asset_copy TARGET_NAME)
    _setup_runtime_directory_sync(
        ${TARGET_NAME}
        assets
        "${CMAKE_SOURCE_DIR}/assets"
        COPY_IMGUI_INI
    )
endfunction()

# ============================================
# 配置 UI 资源复制（RmlUi 文档/样式/主题）
# 用法：setup_ui_copy(目标名称)
# ============================================
function(setup_ui_copy TARGET_NAME)
    _setup_runtime_directory_sync(
        ${TARGET_NAME}
        ui
        "${CMAKE_SOURCE_DIR}/ui"
    )
endfunction()

# ============================================
# 配置脚本文件复制（Lua 脚本层）
# 用法：setup_script_copy(目标名称)
# ============================================
function(setup_script_copy TARGET_NAME)
    _setup_runtime_directory_sync(
        ${TARGET_NAME}
        scripts
        "${CMAKE_SOURCE_DIR}/scripts"
    )
endfunction()

# ============================================
# 配置配置文件复制
# 用法：setup_config_copy(目标名称)
# ============================================
function(setup_config_copy TARGET_NAME)
    _setup_runtime_directory_sync(
        ${TARGET_NAME}
        config
        "${CMAKE_SOURCE_DIR}/config"
    )
endfunction()

# ============================================
# 配置Windows DLL复制
# 用法：setup_windows_dll_copy(目标名称)
# ============================================
function(setup_windows_dll_copy TARGET_NAME)
    if(NOT WIN32)
        return()
    endif()
    
    message(STATUS "配置Windows DLL自动检测和复制...")
    
    # 1. 复制prebuilt目录中的预编译DLL（如果存在）
    if(EXISTS ${CMAKE_SOURCE_DIR}/prebuilt/bin)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/prebuilt/bin $<TARGET_FILE_DIR:${TARGET_NAME}>
            COMMENT "Copy prebuilt DLLs to executable directory"
            VERBATIM
        )
    endif()
    
    # 2. 自动检测并复制所有运行时DLL依赖（CMake 3.21+）
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
        # 使用独立的脚本模块，但DLL_LIST必须用file(GENERATE)展开
        # 因为$<TARGET_RUNTIME_DLLS>只能在生成阶段解析
        set(COPY_SCRIPT ${CMAKE_SOURCE_DIR}/cmake/scripts/CopyDLLs.cmake)
        
        # 将目标名加入生成文件名以避免多目标在多配置生成器中写入相同文件
        file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/copy_dlls_wrapper_${TARGET_NAME}_$<CONFIG>.cmake CONTENT "
# DLL复制包装脚本 - 用于展开生成器表达式
set(DLL_LIST \"$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>\")
set(TARGET_DIR \"$<TARGET_FILE_DIR:${TARGET_NAME}>\")

# 直接包含脚本文件，避免通过命令行参数传递列表
include(\"${COPY_SCRIPT}\")
")
        
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/copy_dlls_wrapper_${TARGET_NAME}_$<CONFIG>.cmake
            COMMENT "Auto-detect and copy runtime DLLs"
            VERBATIM
        )
    else()
        message(WARNING "CMake版本 < 3.21，无法自动复制运行时DLL。")
        message(WARNING "如果使用了动态库，请手动复制DLL到exe目录。")
    endif()
endfunction()
