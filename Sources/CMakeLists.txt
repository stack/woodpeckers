#
# Library Definition
#

set(LIBRARY_SOURCES )
list(APPEND LIBRARY_SOURCES "${CMAKE_BINARY_DIR}/config.h")
list(APPEND LIBRARY_SOURCES "${CMAKE_CURRENT_LIST_DIR}/EventLoop.h")

if (TARGET_PLATFORM_APPLE)
    list(APPEND LIBRARY_SOURCES "${CMAKE_CURRENT_LIST_DIR}/KqueueEventLoop.c")
elseif (TARGET_PLATFORM_LINUX)
    list(APPEND LIBRARY_SOURCES "${CMAKE_CURRENT_LIST_DIR}/EpollEventLoop.c")
endif()

add_library(Woodpeckers ${LIBRARY_SOURCES})

target_include_directories(Woodpeckers PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(Woodpeckers PUBLIC PkgConfig::YAML)

#
# Application Definition
#

set(APPLICATION_SOURCES )
list(APPEND APPLICATION_SOURCES "${CMAKE_BINARY_DIR}/config.h")
list(APPEND APPLICATION_SOURCES "${CMAKE_CURRENT_LIST_DIR}/main.c")

add_executable(Woodpeckers-bin ${APPLICATION_SOURCES})

target_link_libraries(Woodpeckers-bin Woodpeckers)

set_target_properties(Woodpeckers-bin PROPERTIES OUTPUT_NAME Woodpeckers)
