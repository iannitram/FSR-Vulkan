cmake_minimum_required(VERSION 3.16)
project(uscale_minimal C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# This consumer smoke test only calls the platform independent API. Games
# using uscale_vulkan.h should leave USCALE_WITH_VULKAN enabled instead.
set(USCALE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(USCALE_WITH_VULKAN OFF CACHE BOOL "" FORCE)
add_subdirectory(../.. uscale-build)

add_executable(uscale_minimal main.c)
target_link_libraries(uscale_minimal PRIVATE uscale::uscale)
