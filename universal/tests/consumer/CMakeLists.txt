# Tiny consumer project testing find_package(uscale). SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.16)
project(uscale_consumer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(uscale CONFIG REQUIRED)

add_executable(uscale_consumer consumer.c)
target_link_libraries(uscale_consumer PRIVATE uscale::uscale)

enable_testing()
add_test(NAME uscale_consumer COMMAND uscale_consumer)
