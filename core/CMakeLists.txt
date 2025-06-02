cmake_minimum_required(VERSION 3.10)
project(ecewo VERSION 0.24.0 LANGUAGES C)

# Platform definitions
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
endif()

include(FetchContent)

# Empty place for TinyCBOR (do not touch this comment line)

# Fetch libuv from the GitHub repository
FetchContent_Declare(
  libuv
  GIT_REPOSITORY https://github.com/libuv/libuv.git
  GIT_TAG v1.51.0
)

# Make libuv available to the project
FetchContent_MakeAvailable(libuv)

# Fetch llhttp from the GitHub repository
FetchContent_Declare(
  llhttp
  URL "https://github.com/nodejs/llhttp/archive/refs/tags/release/v8.1.0.tar.gz"
)

# Disable shared library build and enable static library build
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(BUILD_STATIC_LIBS ON CACHE INTERNAL "")

# Make llhttp available to the project
FetchContent_MakeAvailable(llhttp)

# Function to patch cmake_minimum_required version in CMakeLists.txt files
function(patch_cmake_minimum_required target_file component_name)
    if(EXISTS "${target_file}")
        # Read the file
        file(READ "${target_file}" raw_content)

        # Extract current cmake_minimum_required version
        string(REGEX MATCH 
            "cmake_minimum_required\\([ \t]*VERSION[ \t]*([0-9]+)\\.([0-9]+)\\.*([0-9]*)[ \t]*\\)"
            version_match 
            "${raw_content}"
        )
        
        if(version_match)
            # Parse version components
            set(major_version ${CMAKE_MATCH_1})
            set(minor_version ${CMAKE_MATCH_2})
            set(patch_version ${CMAKE_MATCH_3})
            
            # If patch version is empty, set to 0
            if(NOT patch_version)
                set(patch_version 0)
            endif()
            
            # Create version number for comparison (e.g., 3.5.1 -> 30501)
            math(EXPR current_version "${major_version} * 10000 + ${minor_version} * 100 + ${patch_version}")
            math(EXPR target_version "3 * 10000 + 10 * 100 + 0")  # 3.10.0 -> 31000
            
            # Only patch if current version is less than 3.10
            if(current_version LESS target_version)
                string(REGEX REPLACE
                    "cmake_minimum_required\\([ \t]*VERSION[ \t]*[0-9]+\\.[0-9]+\\.*[0-9]*[ \t]*\\)"
                    "cmake_minimum_required(VERSION 3.10)"
                    patched_content
                    "${raw_content}"
                )
                
                file(WRITE "${target_file}" "${patched_content}")
                message(STATUS "${component_name} CMakeLists.txt patched from ${major_version}.${minor_version}.${patch_version} to 3.10")
            else()
                message(STATUS "${component_name} CMakeLists.txt version ${major_version}.${minor_version}.${patch_version} is already >= 3.10, no patching needed")
            endif()
        else()
            message(WARNING "${component_name} CMakeLists.txt does not contain cmake_minimum_required")
        endif()
    else()
        message(WARNING "${component_name} CMakeLists.txt not found at: ${target_file}")
    endif()
endfunction()

# Empty place for l8w8jwt (do not touch this comment line)

# List of source files (do not touch this comment line)
set(SRC_FILES
    ecewo.c
    lib/request.c
    lib/router.c
    server/server.c
    utils/cors.c
    utils/middleware.c
)

# Create library
add_library(core STATIC ${SRC_FILES})

# Include directories
target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/vendors
    ${CMAKE_CURRENT_SOURCE_DIR}/vendors/l8w8jwt/include
    ${CMAKE_BINARY_DIR}/libuv/include
    ${CMAKE_BINARY_DIR}/llhttp/include
    ${CMAKE_BINARY_DIR}/tinycbor/include
    ${CMAKE_BINARY_DIR}/_deps/libuv-src/src
    ${CMAKE_BINARY_DIR}/_deps/libuv-src/include
    ${CMAKE_BINARY_DIR}/_deps/llhttp-src/src
    ${CMAKE_BINARY_DIR}/_deps/llhttp-src/include
    ${CMAKE_BINARY_DIR}/_deps/tinycbor-src/src
    ${CMAKE_BINARY_DIR}/_deps/tinycbor-src/include
    ${CMAKE_BINARY_DIR}
)

# Create configuration file
set(ECEWO_COPYRIGHT "2025 © Savas Sahin <savashn>")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in"
    "${CMAKE_BINARY_DIR}/ecewo_config.h"
)

# Link libraries
target_link_libraries(core uv llhttp_static)

# Link additional libraries for Windows
if(WIN32)
    target_link_libraries(core wsock32 ws2_32)
endif()

# Compiler settings
if(MSVC)
    # Settings for Visual Studio
    target_compile_options(core PRIVATE /W4)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # Settings for GCC/Clang
    target_compile_options(core PRIVATE -Wall -Wextra -std=gnu11)
endif()

# Debug information
message(STATUS "Configuring ecewo version ${ecewo_VERSION}")
message(STATUS "Target platform: ${CMAKE_SYSTEM_NAME}")
