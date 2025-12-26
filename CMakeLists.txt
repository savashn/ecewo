cmake_minimum_required(VERSION 3.14)
project(ecewo VERSION 2.3.1 LANGUAGES C)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # DOWNLOAD_EXTRACT_TIMESTAMP
endif()

if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW) # Don't add /W3 by default
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build" FORCE)
endif()

option(ECEWO_BUILD_SHARED "Build shared library instead of static" OFF)
option(ECEWO_BUILD_TESTS "Build tests" OFF)

include(FetchContent)

FetchContent_Declare(
    libuv
    GIT_REPOSITORY https://github.com/libuv/libuv.git
    GIT_TAG v1.51.0
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    llhttp
    URL "https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.3.0.tar.gz"
)

if(ECEWO_BUILD_SHARED)
    set(LIBUV_BUILD_SHARED ON CACHE BOOL "" FORCE)
    set(LLHTTP_BUILD_SHARED_LIBS ON CACHE INTERNAL "" FORCE)
    set(LLHTTP_BUILD_STATIC_LIBS OFF CACHE INTERNAL "" FORCE)
    set(BUILD_SHARED_LIBS ON CACHE INTERNAL "" FORCE)
    set(BUILD_STATIC_LIBS OFF CACHE INTERNAL "" FORCE)
else()
    set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(LLHTTP_BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
    set(LLHTTP_BUILD_STATIC_LIBS ON CACHE INTERNAL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE INTERNAL "" FORCE)
endif()

FetchContent_MakeAvailable(libuv llhttp)

if(NOT TARGET ecewo::ecewo)
    set(ECEWO_SOURCES
        src/server.c
        src/http.c
        src/request.c
        src/response.c
        src/router.c
        src/middleware.c
        src/route-trie.c
        src/route-register.c
        src/arena.c
        src/arena-pool.c
        src/spawn.c
        src/utils/date-cache.c
    )

    if(ECEWO_BUILD_SHARED)
        add_library(ecewo SHARED ${ECEWO_SOURCES})
        message(STATUS "Building ecewo as SHARED library")
    else()
        add_library(ecewo STATIC ${ECEWO_SOURCES})
        message(STATUS "Building ecewo as STATIC library")
    endif()

    add_library(ecewo::ecewo ALIAS ecewo)

    target_include_directories(ecewo 
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/libuv-src/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
            ${CMAKE_BINARY_DIR}/_deps/llhttp-src/include
    )

    if(ECEWO_BUILD_SHARED)
        target_link_libraries(ecewo PUBLIC uv llhttp_shared)
    else()
        target_link_libraries(ecewo PUBLIC uv_a llhttp_static)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(ecewo PRIVATE ECEWO_DEBUG=1)
    endif()

    set_target_properties(ecewo PROPERTIES
        OUTPUT_NAME ecewo
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME ecewo
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
    )
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    message(STATUS "=== ECEWO Build Configuration ===")
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "Library type: ${ECEWO_BUILD_SHARED}")
    message(STATUS "Target system: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
    message(STATUS "=================================")
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ecewo PRIVATE
            -Wall
            -Wextra
            -Wstrict-prototypes
        )
    endif()
    
    if(MSVC)
        target_compile_options(ecewo PRIVATE
            /W4
        )
    endif()
endif()

if(ECEWO_BUILD_TESTS)
    enable_testing()

    add_executable(ecewo_test
        tests/runner.c
        tests/ecewo-mock.c
        tests/test-body.c
        tests/test-blocking.c
        tests/test-concurrent-request.c
        tests/test-context.c
        tests/test-fire-and-forget.c
        tests/test-headers.c
        tests/test-methods.c
        tests/test-middleware.c
        tests/test-params.c
        tests/test-query.c
        tests/test-redirect.c
        tests/test-response.c
        tests/test-root.c
        tests/test-task-parallel.c
        tests/test-task.c
    )

    target_include_directories(ecewo_test PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
    )

    target_link_libraries(ecewo_test PRIVATE ecewo)
    
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ecewo_test PRIVATE
            -Wall
            -Wextra
            -Werror
            -Wstrict-prototypes
            -Wincompatible-pointer-types
        )
    endif()
    
    if(MSVC)
        target_compile_options(ecewo_test PRIVATE
            /W4
            /WX
        )
    endif()
    
    add_test(NAME ecewo_all_tests COMMAND ecewo_test)
endif()
