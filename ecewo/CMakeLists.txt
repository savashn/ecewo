cmake_minimum_required(VERSION 3.10)
project(ecewo VERSION 0.15.0 LANGUAGES C)

# Platform definitions
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
endif()

# libuv path setup
set(LIBUV_PATH ${CMAKE_SOURCE_DIR}/ecewo/server/lib/libuv)

# Check if libuv is already built
if(NOT TARGET uv)
    # If libuv is a git submodule or separate source tree
    if(EXISTS "${LIBUV_PATH}/CMakeLists.txt")
        # Include libuv as a subproject
        add_subdirectory(${LIBUV_PATH} ${CMAKE_BINARY_DIR}/libuv)
    else()
        # Try to find installed libuv
        find_package(libuv REQUIRED)
        if(NOT libuv_FOUND)
            message(FATAL_ERROR "libuv not found. Please install libuv or make sure the path is correct.")
        endif()
    endif()
endif()

# List of source files
set(SRC_FILES
    router.c
    server/handler.c
    server/request.c
    server/start/ecewo.c
    server/utils/session.c
    server/utils/middleware.c
    server/utils/async.c
    server/lib/cjson.c
    server/lib/dotenv.c
)

# Create library
add_library(ecewo STATIC ${SRC_FILES})

# Include directories
target_include_directories(ecewo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/server/start
    ${CMAKE_CURRENT_SOURCE_DIR}/server/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/server/lib
    ${LIBUV_PATH}/include  # Add direct include path to libuv headers
    ${CMAKE_BINARY_DIR} # For the generated configuration file
)

# Create configuration file
set(ECEWO_COPYRIGHT "2025 © Savas Sahin <savashn>")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in"
    "${CMAKE_BINARY_DIR}/ecewo_config.h"
)

# Link libraries
target_link_libraries(ecewo uv)  # Link with libuv

# Link additional libraries for Windows
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
