# ============================================
# WebAssembly runtime target helpers
# ============================================

include("${CMAKE_CURRENT_LIST_DIR}/WebPreload.cmake")

set(TF_WEB_PRELOAD_ARGS
    "${CMAKE_SOURCE_DIR}/manifests/assets/web-poc-preload.args"
    CACHE FILEPATH
    "Web POC preload argument manifest"
)

function(tf_configure_web_runtime_packages TARGET_NAME)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(_package_dir "${CMAKE_BINARY_DIR}/web-packages")
    set(_package_index "${_package_dir}/web-package-index.json")
    set(_boot_preload "${CMAKE_BINARY_DIR}/web-boot-preload.args")

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/web_release/package_web_assets.py"
            "--manifest" "${TF_WEB_PRELOAD_ARGS}"
            "--output-dir" "${_package_dir}"
            "--boot-preload-output" "${_boot_preload}"
            "--json-output" "${_package_index}"
        COMMENT "Generate Web runtime asset packages"
        VERBATIM
    )
endfunction()

function(tf_apply_web_compile_options TARGET_NAME)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "tf_apply_web_compile_options requires Emscripten.")
    endif()

    target_compile_options(${TARGET_NAME}
        PRIVATE
            -fno-exceptions
    )
    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            SPDLOG_NO_EXCEPTIONS
            JSON_NOEXCEPTION
            SOL_NO_EXCEPTIONS=1
    )
endfunction()

function(tf_configure_web_executable TARGET_NAME)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "tf_configure_web_executable requires Emscripten.")
    endif()

    tf_apply_web_compile_options(${TARGET_NAME})

    target_link_options(${TARGET_NAME}
        PRIVATE
            -sUSE_SDL=3
            -sMIN_WEBGL_VERSION=2
            -sMAX_WEBGL_VERSION=2
            -sALLOW_MEMORY_GROWTH=1
            -sSTACK_SIZE=1048576
            -sINITIAL_MEMORY=134217728
            -sFORCE_FILESYSTEM=1
            -sEXIT_RUNTIME=0
            -sDISABLE_EXCEPTION_CATCHING=1
            -lidbfs.js
    )

    if(TF_WEB_ENABLE_PTHREADS)
        target_compile_options(${TARGET_NAME}
            PRIVATE
                -pthread
        )
        target_link_options(${TARGET_NAME}
            PRIVATE
                -pthread
                -sUSE_PTHREADS=1
                -sPTHREAD_POOL_SIZE=2
        )
    endif()

    tf_target_web_preload(${TARGET_NAME} "${TF_WEB_PRELOAD_ARGS}")

    if(TF_WEB_ENABLE_RUNTIME_PACKAGES)
        tf_configure_web_runtime_packages(${TARGET_NAME})
    endif()

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/favicon.ico"
        COMMENT "Create empty favicon for local Web smoke"
    )

    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "TinyFarmRPG-Web"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        SUFFIX ".html"
    )
endfunction()
