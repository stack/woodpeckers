#
# Project Set Up
#

cmake_minimum_required(VERSION 3.18)

set(TARGET_PLATFORM "Null" CACHE STRING "Target Platform")

if ("${TARGET_PLATFORM}" STREQUAL "Linux")
    set(TARGET_PLATFORM_LINUX ON)
elseif("${TARGET_PLATFORM}" STREQUAL "macOS")
    set(TARGET_PLATFORM_APPLE ON)
    set(TARGET_PLATFORM_MACOS ON)
else()
    message(FATAL_ERROR "Target platform \"${TARGET_PLATFORM}\" is not supported")
endif()

project(Woodpeckers VERSION "0.1.0.1" LANGUAGES C CXX)

#
# Compilation Options
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
    add_compile_options(-Wall -Wpedantic -Werror)
elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Wpedantic -Werror)
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/config.h.in ${CMAKE_BINARY_DIR}/config.h)

#
# Third-Party Libraries
#

find_package(PkgConfig REQUIRED)
pkg_check_modules(YAML REQUIRED IMPORTED_TARGET yaml-0.1)

#
# Subdirectories
#

add_subdirectory(Sources)

enable_testing()

find_package(GTest REQUIRED)

include(GoogleTest)
include(CTest)

add_subdirectory(Tests)