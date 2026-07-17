# ============================================
# 运行时目录增量同步脚本
# ============================================
# 必需参数：SOURCE_DIR、TARGET_DIR
# 可选参数：EXTRA_SOURCE_FILE、EXTRA_TARGET_FILE

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED TARGET_DIR)
    message(FATAL_ERROR "Required parameters: SOURCE_DIR and TARGET_DIR")
endif()
if(NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "Source directory does not exist: ${SOURCE_DIR}")
endif()

get_filename_component(TARGET_PARENT "${TARGET_DIR}" DIRECTORY)
get_filename_component(TARGET_NAME "${TARGET_DIR}" NAME)
file(MAKE_DIRECTORY "${TARGET_PARENT}")

# 多个可执行目标可能共享同一输出目录，使用目标目录级锁串行化同步。
set(LOCK_FILE "${TARGET_PARENT}/.tf_${TARGET_NAME}_sync.lock")
file(LOCK "${LOCK_FILE}" GUARD PROCESS TIMEOUT 60)

file(MAKE_DIRECTORY "${TARGET_DIR}")
set(MANIFEST_FILE "${TARGET_DIR}/.tf_source_manifest")
set(PREVIOUS_FILES)
if(EXISTS "${MANIFEST_FILE}")
    file(STRINGS "${MANIFEST_FILE}" PREVIOUS_FILES)
endif()

file(GLOB_RECURSE SOURCE_FILES
    LIST_DIRECTORIES FALSE
    RELATIVE "${SOURCE_DIR}"
    "${SOURCE_DIR}/*"
)
list(SORT SOURCE_FILES)

set(COPIED_COUNT 0)
foreach(REL_PATH IN LISTS SOURCE_FILES)
    set(SOURCE_FILE "${SOURCE_DIR}/${REL_PATH}")
    set(TARGET_FILE "${TARGET_DIR}/${REL_PATH}")
    get_filename_component(TARGET_FILE_DIR "${TARGET_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${TARGET_FILE_DIR}")

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SOURCE_FILE}" "${TARGET_FILE}"
        RESULT_VARIABLE COPY_RESULT
    )
    if(NOT COPY_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to copy runtime file: ${SOURCE_FILE}")
    endif()
    math(EXPR COPIED_COUNT "${COPIED_COUNT} + 1")
endforeach()

# 只删除上一次由本脚本复制、但源目录中已经不存在的文件，保留运行时生成文件。
set(REMOVED_COUNT 0)
foreach(REL_PATH IN LISTS PREVIOUS_FILES)
    list(FIND SOURCE_FILES "${REL_PATH}" SOURCE_INDEX)
    if(SOURCE_INDEX EQUAL -1)
        set(STALE_FILE "${TARGET_DIR}/${REL_PATH}")
        if(EXISTS "${STALE_FILE}" AND NOT IS_DIRECTORY "${STALE_FILE}")
            file(REMOVE "${STALE_FILE}")
            math(EXPR REMOVED_COUNT "${REMOVED_COUNT} + 1")
        endif()
    endif()
endforeach()

string(REPLACE ";" "\n" MANIFEST_CONTENT "${SOURCE_FILES}")
if(NOT MANIFEST_CONTENT STREQUAL "")
    string(APPEND MANIFEST_CONTENT "\n")
endif()
file(WRITE "${MANIFEST_FILE}" "${MANIFEST_CONTENT}")

if(DEFINED EXTRA_SOURCE_FILE AND DEFINED EXTRA_TARGET_FILE AND EXISTS "${EXTRA_SOURCE_FILE}")
    get_filename_component(EXTRA_TARGET_DIR "${EXTRA_TARGET_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${EXTRA_TARGET_DIR}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${EXTRA_SOURCE_FILE}" "${EXTRA_TARGET_FILE}"
        RESULT_VARIABLE EXTRA_COPY_RESULT
    )
    if(NOT EXTRA_COPY_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to copy runtime file: ${EXTRA_SOURCE_FILE}")
    endif()
endif()

message(STATUS "Synced ${COPIED_COUNT} file(s), removed ${REMOVED_COUNT}: ${SOURCE_DIR} -> ${TARGET_DIR}")
