cmake_minimum_required(VERSION 3.16)
project(obs_overlay_net_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET obs_overlay_client)
    add_subdirectory(../../implementation implementation_build)
endif()

add_executable(obs_overlay_net_server src/main.cpp)

target_include_directories(obs_overlay_net_server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../shared"
)
target_link_libraries(obs_overlay_net_server PRIVATE
    obs_overlay_client
    ws2_32
)

if(MSVC)
    target_compile_options(obs_overlay_net_server PRIVATE /W4 /permissive-)
endif()
