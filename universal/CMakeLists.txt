# uscale: our shared upscaler and frame-generation library (SPEC.md, docs/code-map.md).
# Builds standalone: cmake -S universal -B build-uscale
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.16)
project(uscale VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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
option(USCALE_TEST_FSR3_FRAMEGEN "Run the FSR 3.1.6 GPU allocation lifetime regression" OFF)

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    target_link_libraries(uscale_vulkan PUBLIC uscale_core Vulkan::Vulkan PRIVATE ${CMAKE_DL_LIBS})
    uscale_warnings(uscale_vulkan)
    add_library(uscale::uscale ALIAS uscale_vulkan)
else()
    add_library(uscale::uscale ALIAS uscale_core)
endif()

if(USCALE_WITH_FFX OR (USCALE_BUILD_TESTS AND USCALE_TEST_FSR3_FRAMEGEN))
    if(NOT USCALE_WITH_VULKAN)
        message(FATAL_ERROR "USCALE_WITH_FFX and USCALE_TEST_FSR3_FRAMEGEN need USCALE_WITH_VULKAN=ON")
    endif()
    # Enable C++ here so our C consumers can link the upstream C++ libraries.
    # The normal uscale build still needs only a C11 compiler.
    enable_language(CXX)

    # Share one upstream tree with the opt-in frame-generation regression.
    # Force these even for a reused cache: upstream's tests/probe/install must
    # stay off, and the SDK 2.3 scaffold supplies the 3.1.6 FI/OF target.
    set(FFX_VK_PORTABLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FFX_VK_PORTABLE_BUILD_PROBE OFF CACHE BOOL "" FORCE)
    set(FFX_VK_PORTABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(FFX_VK_PORTABLE_BUILD_FSR3_3_1_5_SCAFFOLD ON CACHE BOOL "" FORCE)
    # The regression alone retains its smaller frame-generation-only closure.
    set(FFX_VK_PORTABLE_BUILD_FSR3_HOST ${USCALE_WITH_FFX} CACHE BOOL "" FORCE)
    set(FFX_VK_PORTABLE_BUILD_FSR3_VK_BACKEND ${USCALE_WITH_FFX} CACHE BOOL "" FORCE)
    set(FFX_VK_PORTABLE_BUILD_FSR4_V07_VULKAN ${USCALE_WITH_FFX} CACHE BOOL "" FORCE)

    # Upstream also defines recovered targets unconditionally. Excluding its
    # ALL target and linking individual FSR targets keeps those unbuilt
    # (AGENTS.fork.md, ruling 2026-11-01). Never link its effects umbrella.
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." upstream-fsr EXCLUDE_FROM_ALL)
endif()

if(USCALE_WITH_FFX)
    target_link_libraries(uscale_vulkan PRIVATE
        $<BUILD_INTERFACE:ffx-vulkan::fsr3-vk-backend-1.1.4>
        $<BUILD_INTERFACE:ffx-vulkan::fsr3-vk-framegeneration-3.1.6>
        $<BUILD_INTERFACE:ffx-vulkan::framegeneration-presenter-policy>
        $<BUILD_INTERFACE:ffx-vulkan::fsr4-v07-vulkan>)
endif()

if(USCALE_BUILD_TESTS)
    enable_testing()
    if(USCALE_TEST_FSR3_FRAMEGEN)
        add_executable(uscale_vk_framegen_retire_test tests/uscale_vk_framegen_retire_test.cpp)
        target_compile_features(uscale_vk_framegen_retire_test PRIVATE cxx_std_20)
        target_link_libraries(uscale_vk_framegen_retire_test PRIVATE
            ffx-vulkan::fsr3-vk-framegeneration-3.1.6)
        uscale_warnings(uscale_vk_framegen_retire_test)
        if(NOT MSVC)
            target_compile_options(uscale_vk_framegen_retire_test PRIVATE -Wno-missing-field-initializers)
        endif()
        add_test(NAME uscale_vk_framegen_retire_test COMMAND uscale_vk_framegen_retire_test)
        set_tests_properties(uscale_vk_framegen_retire_test PROPERTIES TIMEOUT 300)
    endif()
    # Every tests/*_test.c is one test. uscale_vk_*_test.c needs the Vulkan half.
    add_library(uscale_runtime_fixture SHARED tests/runtime_loader_fixture.c)
    uscale_warnings(uscale_runtime_fixture)
    if(WIN32)
        set_target_properties(uscale_runtime_fixture PROPERTIES OUTPUT_NAME nvngx_dlss)
    else()
        set_target_properties(uscale_runtime_fixture PROPERTIES
            PREFIX "lib" OUTPUT_NAME "nvidia-ngx-dlss.so.310.9.1" SUFFIX "")
    endif()
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
        if(_test_name STREQUAL "uscale_vk_fsr3_dispatch_test" OR _test_name STREQUAL "uscale_vk_context_test")
            # Internal headers (backend.h, backend_fsr3.h): no FFX build or GPU needed.
            target_include_directories(${_test_name} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/../include)
        endif()
        uscale_warnings(${_test_name})
        if(_test_name STREQUAL "uscale_runtime_loader_test")
            # Exercise the loader even on CI jobs without the Vulkan SDK.
            target_sources(${_test_name} PRIVATE src/runtime_loader.c)
            target_include_directories(${_test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
            target_link_libraries(${_test_name} PRIVATE ${CMAKE_DL_LIBS})
            add_dependencies(${_test_name} uscale_runtime_fixture)
            add_test(NAME ${_test_name} COMMAND ${_test_name} $<TARGET_FILE:uscale_runtime_fixture>)
        elseif(_test_name STREQUAL "uscale_vk_dlss_runtime_test")
            target_link_libraries(${_test_name} PRIVATE ${CMAKE_DL_LIBS})
            target_include_directories(${_test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
            add_dependencies(${_test_name} uscale_runtime_fixture)
            file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/dlss_runtime_search)
            add_test(NAME ${_test_name} COMMAND ${_test_name}
                $<TARGET_FILE:uscale_runtime_fixture>
                ${CMAKE_CURRENT_BINARY_DIR}/dlss_runtime_search)
        elseif(_test_name STREQUAL "uscale_vk_caps_test")
            # Empty folder for the FSR 4 asset check; the source include folder is present.
            file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fsr4_empty)
            add_test(NAME ${_test_name} COMMAND ${_test_name}
                ${CMAKE_CURRENT_BINARY_DIR}/fsr4_empty ${CMAKE_CURRENT_SOURCE_DIR}/include)
        elseif(_test_name STREQUAL "uscale_vk_caps_runtime_test")
            set(_runtime_tmp ${CMAKE_CURRENT_BINARY_DIR}/xess_runtime_tmp)
            file(MAKE_DIRECTORY ${_runtime_tmp})
            add_dependencies(${_test_name} uscale_runtime_fixture)
            add_test(NAME ${_test_name} COMMAND ${_test_name}
                $<TARGET_FILE:uscale_runtime_fixture> ${_runtime_tmp})
            set_tests_properties(${_test_name} PROPERTIES
                ENVIRONMENT "DYLD_LIBRARY_PATH=${_runtime_tmp};LD_LIBRARY_PATH=${_runtime_tmp};PATH=${_runtime_tmp}")
        else()
            add_test(NAME ${_test_name} COMMAND ${_test_name})
        endif()
        if(_test_name STREQUAL "uscale_vk_device_test" OR _test_name STREQUAL "uscale_vk_caps_test")
            set_tests_properties(${_test_name} PROPERTIES SKIP_RETURN_CODE 77)
        endif()
    endforeach()
endif()

# Install and package export rules (SPEC M12).
install(TARGETS uscale_core
    EXPORT uscaleTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(USCALE_WITH_VULKAN)
    install(TARGETS uscale_vulkan
        EXPORT uscaleTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

install(FILES include/uscale.h include/uscale_vulkan.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT uscaleTargets
    FILE uscaleTargets.cmake
    NAMESPACE uscale::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uscale
)

configure_package_config_file(
    cmake/uscaleConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/uscaleConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uscale
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/uscaleConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/uscaleConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/uscaleConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uscale
)
