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

project(Woodpeckers VERSION "0.1.0.1" LANGUAGES C)

#
# Compilation Options
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
    add_compile_options(-Wall -Wpedantic -Werror)
elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Wpedantic -Werror)
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/config.h.in ${CMAKE_BINARY_DIR}/config.h)

#
# Application Definition
#

set(APPLICATION_SOURCES )
list(APPEND APPLICATION_SOURCES "${CMAKE_BINARY_DIR}/config.h")
list(APPEND APPLICATION_SOURCES "${CMAKE_CURRENT_LIST_DIR}/Sources/EventLoop.h")
list(APPEND APPLICATION_SOURCES "${CMAKE_CURRENT_LIST_DIR}/Sources/main.c")

if (TARGET_PLATFORM_APPLE)
    list(APPEND APPLICATION_SOURCES "${CMAKE_CURRENT_LIST_DIR}/Sources/KqueueEventLoop.c")
elseif (TARGET_PLATFORM_LINUX)
    list(APPEND APPLICATION_SOURCES "${CMAKE_CURRENT_LIST_DIR}/Sources/EpollEventLoop.c")
endif()

add_executable(Woodpeckers ${APPLICATION_SOURCES})

target_include_directories(Woodpeckers PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(Woodpeckers PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Sources)

