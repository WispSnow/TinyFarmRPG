# ============================================
# Web preload manifest helpers
# ============================================

function(tf_target_web_preload TARGET_NAME MANIFEST_PATH)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "tf_target_web_preload requires Emscripten.")
    endif()

    if(IS_ABSOLUTE "${MANIFEST_PATH}")
        set(_manifest "${MANIFEST_PATH}")
    else()
        set(_manifest "${CMAKE_SOURCE_DIR}/${MANIFEST_PATH}")
    endif()

    if(NOT EXISTS "${_manifest}")
        message(FATAL_ERROR "Web preload manifest not found: ${_manifest}")
    endif()

    file(STRINGS "${_manifest}" _preload_lines ENCODING UTF-8)
    set(_stage_dir "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-preload-root")
    set(_stamp_file "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-preload.stamp")
    set(_preload_count 0)
    set(_preload_sources)

    foreach(_raw_line IN LISTS _preload_lines)
        string(STRIP "${_raw_line}" _line)
        if(_line STREQUAL "" OR _line MATCHES "^#")
            continue()
        endif()

        if(_line MATCHES "^--preload-file[ \t]+(.+)$")
            set(_payload "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "^--preload-file=(.+)$")
            set(_payload "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR "Unsupported web preload arg in ${_manifest}: ${_line}")
        endif()

        if(_payload MATCHES "^(.+)@(.+)$")
            set(_source_path "${CMAKE_MATCH_1}")
            set(_mount_path "${CMAKE_MATCH_2}")
        else()
            set(_source_path "${_payload}")
            set(_mount_path "")
        endif()

        if(IS_ABSOLUTE "${_source_path}")
            set(_resolved_source "${_source_path}")
        else()
            set(_resolved_source "${CMAKE_SOURCE_DIR}/${_source_path}")
        endif()

        list(APPEND _preload_sources "${_resolved_source}")
        math(EXPR _preload_count "${_preload_count} + 1")
    endforeach()

    if(_preload_count EQUAL 0)
        message(FATAL_ERROR "Web preload manifest is empty: ${_manifest}")
    endif()

    add_custom_command(
        OUTPUT "${_stamp_file}"
        COMMAND ${CMAKE_COMMAND}
            -D "TF_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -D "TF_MANIFEST=${_manifest}"
            -D "TF_STAGE_DIR=${_stage_dir}"
            -D "TF_STAMP=${_stamp_file}"
            -P "${CMAKE_SOURCE_DIR}/cmake/scripts/StageWebPreload.cmake"
        DEPENDS
            "${_manifest}"
            "${CMAKE_SOURCE_DIR}/cmake/scripts/StageWebPreload.cmake"
            ${_preload_sources}
        VERBATIM
    )

    add_custom_target(${TARGET_NAME}_web_preload DEPENDS "${_stamp_file}")
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_web_preload)

    target_link_options(${TARGET_NAME} PRIVATE "--preload-file=${_stage_dir}@/")
    set_property(TARGET ${TARGET_NAME} APPEND PROPERTY LINK_DEPENDS "${_manifest}")
    set_property(TARGET ${TARGET_NAME} APPEND PROPERTY LINK_DEPENDS "${_stamp_file}")
    set_property(TARGET ${TARGET_NAME} PROPERTY TF_WEB_PRELOAD_COUNT "${_preload_count}")
    message(STATUS "Web preload manifest: ${_manifest} (${_preload_count} files, stage: ${_stage_dir})")
endfunction()
