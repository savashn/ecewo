cmake_minimum_required(VERSION 3.14)
project(ecewo VERSION 0.32.1 LANGUAGES C)

# Platform definitions
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DPLATFORM_LINUX)
endif()

include(FetchContent)

# Fetch libuv from the GitHub repository
FetchContent_Declare(
  libuv
  GIT_REPOSITORY https://github.com/libuv/libuv.git
  GIT_TAG v1.51.0
)

# Make libuv available to the project
FetchContent_MakeAvailable(libuv)

# Fetch llhttp from the GitHub repository
FetchContent_Declare(llhttp
  URL "https://github.com/nodejs/llhttp/archive/refs/tags/release/v8.1.0.tar.gz")

# Static build configuration for llhttp
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(BUILD_STATIC_LIBS ON CACHE INTERNAL "")

FetchContent_MakeAvailable(llhttp)

# Prevent multiple inclusion when used as a subdirectory
if(NOT TARGET ecewo)

    # List of source files
    set(ECEWO_SRC_FILES
        src/server/ecewo.c
        src/server/server.c
        src/lib/request.c
        src/lib/router.c
        src/lib/middleware.c
        src/utils/cors.c
        src/utils/async.c
        src/utils/cookie.c
    )

    # Make source files relative to CMAKE_CURRENT_SOURCE_DIR
    list(TRANSFORM ECEWO_SRC_FILES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

    # Create library
    add_library(ecewo STATIC ${ECEWO_SRC_FILES})

    # Include directories
    target_include_directories(ecewo PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/server>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/utils>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/config>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/libuv-src/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/llhttp-src/include>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/ecewo>
    )

    # Private include directories for internal dependencies
    target_include_directories(ecewo PRIVATE
        ${CMAKE_BINARY_DIR}/_deps/libuv-src/src
        ${CMAKE_BINARY_DIR}/_deps/llhttp-src/src
    )

    # Link libraries
    target_link_libraries(ecewo PUBLIC uv_a llhttp_static)

    # Link additional libraries for Windows
    if(WIN32)
        target_link_libraries(ecewo PRIVATE wsock32 ws2_32)
    endif()

    # Compiler settings
    if(MSVC)
        # Settings for Visual Studio
        target_compile_options(ecewo PRIVATE /W4)
        target_compile_definitions(ecewo PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        # Settings for GCC/Clang
        target_compile_options(ecewo PRIVATE -Wall -Wextra -std=gnu11)
    endif()

    # Set target properties
    set_target_properties(ecewo PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME ecewo
    )

    # Install rules (only if this is the main project)
    if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        include(GNUInstallDirs)
        
        # Install the library
        install(TARGETS ecewo
            EXPORT ecewoTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )
        
        # Install header files
        install(DIRECTORY src/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ecewo
            FILES_MATCHING PATTERN "*.h"
        )
        
        # Install llhttp headers (needed for public API)
        install(DIRECTORY ${llhttp_SOURCE_DIR}/include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ecewo
            FILES_MATCHING PATTERN "*.h"
        )
        
        # Install export targets
        install(EXPORT ecewoTargets
            FILE ecewoTargets.cmake
            NAMESPACE ecewo::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ecewo
        )
    endif()

endif()
