# ============================================
# 资源文件复制脚本
# ============================================
# 此脚本在构建时执行，用于复制资源文件到可执行文件目录
# 使用方式：cmake -DSOURCE_DIR=... -DTARGET_DIR=... -P CopyAssets.cmake

# 检查必需参数
if(NOT DEFINED SOURCE_DIR OR NOT DEFINED TARGET_DIR)
    message(FATAL_ERROR "Required parameters: SOURCE_DIR and TARGET_DIR")
endif()

# 确保目标目录存在
file(MAKE_DIRECTORY "${TARGET_DIR}")

# 避免并行构建时多个目标同时复制到同一目录导致竞争
file(LOCK "${TARGET_DIR}/.copy_assets.lock" GUARD PROCESS TIMEOUT 60)

# 直接复制整个源目录到目标目录（递归复制所有文件和子目录）
file(COPY "${SOURCE_DIR}/" DESTINATION "${TARGET_DIR}")

message(STATUS "Assets copied from ${SOURCE_DIR} to ${TARGET_DIR}")

# 复制 imgui.ini（如果指定且存在）
if(DEFINED IMGUI_INI_SOURCE AND EXISTS "${IMGUI_INI_SOURCE}")
    file(COPY "${IMGUI_INI_SOURCE}" DESTINATION "${TARGET_DIR}")
    message(STATUS "Copy imgui.ini to executable directory")
endif()
