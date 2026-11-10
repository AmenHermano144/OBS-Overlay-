cmake_minimum_required(VERSION 3.16)
project(obs_overlay_net_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(obs_overlay_net_client src/main.cpp)

target_include_directories(obs_overlay_net_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../shared"
)
target_link_libraries(obs_overlay_net_client PRIVATE ws2_32)

if(MSVC)
    target_compile_options(obs_overlay_net_client PRIVATE /W4 /permissive-)
endif()
