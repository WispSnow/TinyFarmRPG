cmake_minimum_required(VERSION 3.21)

project(TinyFarmRPGWasmSmoke LANGUAGES CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "tools/wasm_smoke must be configured with emcmake.")
endif()

add_executable(tinyfarm_wasm_sdl3_smoke sdl3_minimal.cpp)

target_compile_features(tinyfarm_wasm_sdl3_smoke PRIVATE cxx_std_20)
target_compile_options(tinyfarm_wasm_sdl3_smoke PRIVATE -sUSE_SDL=3)
target_link_options(tinyfarm_wasm_sdl3_smoke PRIVATE
    -sUSE_SDL=3
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=1048576
    -sINITIAL_MEMORY=67108864
)

set_target_properties(tinyfarm_wasm_sdl3_smoke PROPERTIES
    OUTPUT_NAME "sdl3_minimal"
    SUFFIX ".html"
)
