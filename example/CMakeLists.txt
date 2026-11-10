cmake_minimum_required(VERSION 3.16)
project(obs_overlay_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET obs_overlay_client)
    add_subdirectory(../implementation implementation_build)
endif()

add_executable(obs_overlay_example src/main.cpp)
target_link_libraries(obs_overlay_example PRIVATE obs_overlay_client)

if(MSVC)
    target_compile_options(obs_overlay_example PRIVATE /W4 /permissive-)
endif()
