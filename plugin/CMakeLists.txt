cmake_minimum_required(VERSION 3.16)
project(obs-hidden-chat-overlay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "Windows only")
endif()

if(NOT DEFINED LIBOBS_PATH)
    message(FATAL_ERROR "Pass -DLIBOBS_PATH=<path to obs-studio source or SDK root>")
endif()

find_library(LIBOBS_LIB
    NAMES obs libobs
    PATHS
        "${LIBOBS_PATH}/lib"
        "${LIBOBS_PATH}/build/libobs/RelWithDebInfo"
        "${LIBOBS_PATH}/build64/libobs/RelWithDebInfo"
    REQUIRED
)

add_library(${PROJECT_NAME} MODULE
    src/plugin-main.cpp
    src/overlay-window.cpp
    src/overlay-renderer.cpp
    src/ipc-receiver.cpp
    src/source-registrar.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${LIBOBS_PATH}"
    "${LIBOBS_PATH}/libobs"
    src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${LIBOBS_LIB}
    d3d11
    dxgi
    d2d1
    dwrite
    dcomp
    dwmapi
    bcrypt
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "obs-hidden-chat-overlay"
    PREFIX ""
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /Zc:__cplusplus /MP)
    target_link_options(${PROJECT_NAME} PRIVATE /SUBSYSTEM:WINDOWS)
endif()
