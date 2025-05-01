cmake_minimum_required(VERSION 3.10)
project(ecewo VERSION 0.12.0 LANGUAGES C)

# Platform definitions
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
endif()

# List of source files
set(SRC_FILES
    server.c
    router.c
    methods.c
    request.c
    lib/session.c
    lib/cjson.c
)

# Create library
add_library(ecewo STATIC ${SRC_FILES})

# Include directories
target_include_directories(ecewo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_BINARY_DIR} # For the generated configuration file
)

# Create configuration file
set(ECEWO_COPYRIGHT "2025 © Savas Sahin <savashn>")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_BINARY_DIR}/ecewo_config.h"
)

# Link libraries for Windows
if(WIN32)
    target_link_libraries(ecewo wsock32 ws2_32)
endif()

# Compiler settings
if(MSVC)
    # Settings for Visual Studio
    target_compile_options(ecewo PRIVATE /W4)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # Settings for GCC/Clang
    target_compile_options(ecewo PRIVATE -Wall -Wextra -std=c99)
endif()

# Debug information
message(STATUS "Configuring ecewo library version ${ecewo_VERSION}")
message(STATUS "Target platform: ${CMAKE_SYSTEM_NAME}")
