# uscale: our shared upscaler and frame-generation library (SPEC.md, docs/code-map.md).
# Builds standalone: cmake -S universal -B build-uscale
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.16)
project(uscale VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Vulkan QUIET)
set(_uscale_vulkan_default OFF)
if(Vulkan_FOUND)
    set(_uscale_vulkan_default ON)
endif()

option(USCALE_BUILD_TESTS "Build the uscale tests" ON)
option(USCALE_WITH_VULKAN "Build the Vulkan half of the API (needs the Vulkan headers)" ${_uscale_vulkan_default})
option(USCALE_WITH_FFX "Link upstream's FSR 3 / FSR 4 v07 targets from the repo root" OFF)

# Our lint: warnings are errors on every compiler.
function(uscale_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

# Pure half: no Vulkan types, builds everywhere.
add_library(uscale_core STATIC
    src/uscale_version.c
    src/uscale_names.c
    src/uscale_render_extent.c
    src/uscale_select.c
    src/uscale_profile.c)
target_include_directories(uscale_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
uscale_warnings(uscale_core)
add_library(uscale::core ALIAS uscale_core)

if(USCALE_WITH_VULKAN)
    if(NOT Vulkan_FOUND)
        message(FATAL_ERROR "USCALE_WITH_VULKAN=ON needs the Vulkan headers and loader")
    endif()
    add_library(uscale_vulkan STATIC
        src/uscale_context.c
        src/uscale_caps.c
        src/runtime_loader.c
        src/backend_fsr3.c
        src/backend_fsr4.c
        src/backend_xess.c
        src/backend_dlss.c
        src/framegen_fsr3.c)
    target_include_directories(uscale_vulkan PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(uscale_vulkan PUBLIC uscale_core Vulkan::Vulkan)
    uscale_warnings(uscale_vulkan)
    add_library(uscale::uscale ALIAS uscale_vulkan)
else()
    add_library(uscale::uscale ALIAS uscale_core)
endif()

if(USCALE_WITH_FFX)
    # Planned (ROADMAP.md stage 2): add the repo root with EXCLUDE_FROM_ALL and
    # link only the FSR 3, frame-generation, presenter and FSR 4 v07 targets.
    # Never xess-recovered or dlss-contract (AGENTS.fork.md, ruling 2026-11-01).
    message(FATAL_ERROR "USCALE_WITH_FFX is not wired yet (see ROADMAP.md stage 2)")
endif()

if(USCALE_BUILD_TESTS)
    enable_testing()
    # Every tests/*_test.c is one test. uscale_vk_*_test.c needs the Vulkan half.
    file(GLOB _uscale_tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*_test.c)
    foreach(_test_src IN LISTS _uscale_tests)
        get_filename_component(_test_name ${_test_src} NAME_WE)
        if(_test_name MATCHES "^uscale_vk_" AND NOT USCALE_WITH_VULKAN)
            continue()
        endif()
        add_executable(${_test_name} ${_test_src})
        target_include_directories(${_test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        if(_test_name MATCHES "^uscale_vk_")
            target_link_libraries(${_test_name} PRIVATE uscale_vulkan)
        else()
            target_link_libraries(${_test_name} PRIVATE uscale_core)
        endif()
        uscale_warnings(${_test_name})
        add_test(NAME ${_test_name} COMMAND ${_test_name})
    endforeach()
endif()
