# ============================================
# 配置文件复制脚本
# ============================================
# 此脚本在构建时执行，用于复制配置文件到可执行文件目录
# 使用方式：cmake -DSOURCE_DIR=... -DTARGET_DIR=... -P CopyConfig.cmake

# 检查必需参数
if(NOT DEFINED SOURCE_DIR OR NOT DEFINED TARGET_DIR)
    message(FATAL_ERROR "Required parameters: SOURCE_DIR and TARGET_DIR")
endif()

# 避免并行构建时多个目标同时复制到同一目录导致竞争
file(MAKE_DIRECTORY "${TARGET_DIR}")
file(LOCK "${TARGET_DIR}/.copy_config.lock" GUARD PROCESS TIMEOUT 60)

# 获取所有源文件
file(GLOB_RECURSE SOURCE_FILES "${SOURCE_DIR}/*")

# 统计实际需要复制的文件数
set(COPIED_COUNT 0)
set(COPIED_FILES "")

foreach(SRC_FILE IN LISTS SOURCE_FILES)
    # 跳过目录
    if(IS_DIRECTORY "${SRC_FILE}")
        continue()
    endif()
    
    if(NOT EXISTS "${SRC_FILE}")
        continue()
    endif()
    
    # 计算相对路径
    file(RELATIVE_PATH REL_PATH "${SOURCE_DIR}" "${SRC_FILE}")
    set(TARGET_FILE "${TARGET_DIR}/${REL_PATH}")
    
    # 检查文件是否需要更新（比较MD5）
    set(NEEDS_COPY FALSE)
    if(EXISTS "${TARGET_FILE}")
        # 目标文件存在，比较内容（MD5哈希）
        file(MD5 "${SRC_FILE}" SRC_MD5)
        file(MD5 "${TARGET_FILE}" DST_MD5)
        if(NOT "${SRC_MD5}" STREQUAL "${DST_MD5}")
            set(NEEDS_COPY TRUE)
        endif()
    else()
        # 目标文件不存在，需要复制
        set(NEEDS_COPY TRUE)
    endif()
    
    if(NEEDS_COPY)
        # 确保目标目录存在
        get_filename_component(TARGET_FILE_DIR "${TARGET_FILE}" DIRECTORY)
        file(MAKE_DIRECTORY "${TARGET_FILE_DIR}")
        
        # 复制配置文件
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_FILE}" "${TARGET_FILE}"
            RESULT_VARIABLE COPY_RESULT
            OUTPUT_QUIET
            ERROR_QUIET
        )
        
        if(COPY_RESULT EQUAL 0)
            math(EXPR COPIED_COUNT "${COPIED_COUNT} + 1")
            list(APPEND COPIED_FILES "  - ${REL_PATH}")
        else()
            message(WARNING "Failed to copy config file: ${SRC_FILE}")
        endif()
    endif()
endforeach()

# 只在有实际复制操作时输出详细信息
if(COPIED_COUNT GREATER 0)
    message(STATUS "Updated ${COPIED_COUNT} config file(s):")
    foreach(FILE IN LISTS COPIED_FILES)
        message(STATUS "${FILE}")
    endforeach()
endif()

