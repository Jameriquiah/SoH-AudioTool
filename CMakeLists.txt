cmake_minimum_required(VERSION 3.20)
project(SoH_AudioTool LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/SDL)

add_library(imgui
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_sdl3.cpp
    vendor/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(imgui PUBLIC
    vendor/imgui
    vendor/imgui/backends
    vendor/SDL/include
)

target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

add_library(vadpcm_codec
    vendor/vadpcm/codec/autocorr.c
    vendor/vadpcm/codec/binary.c
    vendor/vadpcm/codec/codebook.c
    vendor/vadpcm/codec/decode.c
    vendor/vadpcm/codec/encode.c
    vendor/vadpcm/codec/error.c
    vendor/vadpcm/codec/predictor.c
    vendor/vadpcm/codec/random.c
)

target_include_directories(vadpcm_codec PUBLIC
    vendor/vadpcm
)

if (NOT WIN32)
    target_link_libraries(vadpcm_codec PRIVATE m)
endif()

set(SOH_AUDIO_TOOL_SOURCES
    src/main.cpp
    src/AudioFormats.cpp
    src/AudioFormats.h
    src/SohSampleWriter.cpp
    src/SohSampleWriter.h
)

if (WIN32)
    list(APPEND SOH_AUDIO_TOOL_SOURCES
        src/Process.cpp
        src/Process.h
    )
endif()

add_executable(SoH-AudioTool ${SOH_AUDIO_TOOL_SOURCES})

target_include_directories(SoH-AudioTool PRIVATE
    src
    vendor/imgui
    vendor/imgui/backends
    vendor/SDL/include
)

target_link_libraries(SoH-AudioTool PRIVATE imgui SDL3::SDL3 vadpcm_codec)
target_compile_definitions(SoH-AudioTool PRIVATE SDL_MAIN_HANDLED)

if (WIN32)
    target_compile_definitions(SoH-AudioTool PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(SoH-AudioTool PRIVATE comdlg32 shell32 ole32)
endif()
