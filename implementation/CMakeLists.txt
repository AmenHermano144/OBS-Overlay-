cmake_minimum_required(VERSION 3.16)
project(obs_overlay_client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "Windows only")
endif()

add_library(obs_overlay_client STATIC
    src/client.cpp
    src/obs_launcher.cpp
)

target_include_directories(obs_overlay_client PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(obs_overlay_client PUBLIC
    bcrypt
    advapi32
    shell32
)

target_compile_definitions(obs_overlay_client PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(obs_overlay_client PRIVATE /W4 /permissive- /Zc:__cplusplus)
endif()
