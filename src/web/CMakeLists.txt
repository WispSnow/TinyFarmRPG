# ============================================
# WebAssembly shell target
# ============================================

set(TF_WEB_PRELOAD_ARGS
    "${CMAKE_SOURCE_DIR}/manifests/assets/web-poc-preload.args"
    CACHE FILEPATH
    "Web POC preload argument manifest"
)

set(TF_WEB_HTML_SHELL
    "${CMAKE_SOURCE_DIR}/src/web/tinyfarm_web_shell.html"
    CACHE FILEPATH
    "Minimal Web HTML shell"
)

add_executable(${TARGET}
    web_main.cpp
    web_shell_ui.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/platform/filesystem_paths.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/platform/web_persistent_storage.cpp
)

target_compile_features(${TARGET} PRIVATE cxx_std_20)
target_include_directories(${TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/external/stb
)

setup_compiler_options(${TARGET})

target_compile_options(${TARGET}
    PRIVATE
        -fno-exceptions
        -sUSE_SDL=3
)

target_link_options(${TARGET}
    PRIVATE
        -sUSE_SDL=3
        -sMIN_WEBGL_VERSION=2
        -sMAX_WEBGL_VERSION=2
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=1048576
        -sINITIAL_MEMORY=134217728
        -sFORCE_FILESYSTEM=1
        -sEXIT_RUNTIME=0
        -lidbfs.js
        "--shell-file=${TF_WEB_HTML_SHELL}"
)
set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS "${TF_WEB_HTML_SHELL}")

if(TF_WEB_ENABLE_PTHREADS)
    target_compile_options(${TARGET}
        PRIVATE
            -pthread
    )
    target_link_options(${TARGET}
        PRIVATE
            -pthread
            -sUSE_PTHREADS=1
            -sPTHREAD_POOL_SIZE=2
    )
endif()

tf_target_web_preload(${TARGET} "${TF_WEB_PRELOAD_ARGS}")

add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/favicon.ico"
    COMMENT "Create empty favicon for local Web smoke"
)

set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME "TinyFarmRPG-Web"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    SUFFIX ".html"
)
