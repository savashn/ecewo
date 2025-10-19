cmake_minimum_required(VERSION 3.14)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # DOWNLOAD_EXTRACT_TIMESTAMP
endif()
if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW) # Don't add /W3 by default
endif()

project(ecewo VERSION 2.0.0 LANGUAGES C)

# C Standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(UNIX)
    add_compile_definitions(_GNU_SOURCE)
endif()

# Build Type Validation
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build" FORCE)
endif()

if(NOT DEFINED ECEWO_BUILD_SHARED)
    option(ECEWO_BUILD_SHARED "Build shared library instead of static" OFF)
endif()

option(ECEWO_COOKIE "Build cookie module" OFF)
option(ECEWO_SESSION "Build session module" OFF)
option(ECEWO_CORS "Build CORS module" OFF)
option(ECEWO_HELMET "Build helmet module" OFF)
option(ECEWO_FS "Build file serving module" OFF)
option(ECEWO_STATIC "Build static file serving module" OFF)
option(ECEWO_POSTGRES "Build PostgreSQL integration module" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Applying production optimizations for web framework...")
    
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "  - Link Time Optimization (LTO) enabled")
    else()
        message(WARNING "LTO not supported: ${ipo_error}")
    endif()
    
    if(NOT (WIN32 AND MSVC))
        set(CMAKE_STRIP_BINARIES ON)
        message(STATUS "  - Binary stripping enabled")
    endif()
endif()

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
    message(STATUS "  - Configuring libuv as SHARED library")
    message(STATUS "  - Configuring llhttp as SHARED library")
else()
    set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(LLHTTP_BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
    set(LLHTTP_BUILD_STATIC_LIBS ON CACHE INTERNAL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE INTERNAL "" FORCE)
    message(STATUS "  - Configuring libuv as STATIC library")
    message(STATUS "  - Configuring llhttp as STATIC library")
endif()

FetchContent_MakeAvailable(libuv llhttp)

if(NOT TARGET ecewo::ecewo)

    set(ECEWO_SOURCES
        src/server/server.c
        src/server/request.c
        src/lib/router.c
        src/lib/middleware.c
        src/lib/route_trie.c
        src/utils/task.c
        vendors/arena.c
    )

    set(ECEWO_MODULE_DEFINITIONS "")
    set(ECEWO_ENABLED_MODULES "")

    if(ECEWO_COOKIE)
        list(APPEND ECEWO_SOURCES src/modules/cookie.c)
        list(APPEND ECEWO_MODULE_DEFINITIONS ECEWO_HAS_COOKIE)
        list(APPEND ECEWO_ENABLED_MODULES "cookie")
        message(STATUS "  + Cookie module enabled")
    endif()

    if(ECEWO_SESSION)
        list(APPEND ECEWO_SOURCES src/modules/session.c)
        list(APPEND ECEWO_MODULE_DEFINITIONS ECEWO_HAS_SESSION)
        list(APPEND ECEWO_ENABLED_MODULES "session")
        message(STATUS "  + Session module enabled")
    endif()

    if(ECEWO_CORS)
        list(APPEND ECEWO_SOURCES src/modules/cors.c)
        list(APPEND ECEWO_MODULE_DEFINITIONS ECEWO_HAS_CORS)
        list(APPEND ECEWO_ENABLED_MODULES "cors")
        message(STATUS "  + CORS module enabled")
    endif()

    if(ECEWO_HELMET)
        list(APPEND ECEWO_SOURCES src/modules/helmet.c)
        list(APPEND ECEWO_MODULE_DEFINITIONS ECEWO_HAS_HELMET)
        list(APPEND ECEWO_ENABLED_MODULES "helmet")
        message(STATUS "  + Helmet module enabled")
    endif()

    if(ECEWO_FS)
        list(APPEND ECEWO_SOURCES src/modules/fs.c)
        list(APPEND ECEWO_MODULE_DEFINITIONS ECEWO_HAS_FS)
        list(APPEND ECEWO_ENABLED_MODULES "fs")
        message(STATUS "  + File serving module enabled")
    endif()

    if(ECEWO_STATIC)
        list(APPEND ECEWO_SOURCES src/modules/static.c)
        list(APPEND ECEWO_MODULE_DEFINITIONS ECEWO_HAS_STATIC)
        list(APPEND ECEWO_ENABLED_MODULES "static")
        message(STATUS "  + Static file serving module enabled")
    endif()

    if(ECEWO_POSTGRES)
        list(APPEND ECEWO_SOURCES src/modules/postgres.c)
        list(APPEND ECEWO_MODULE_DEFINITIONS ECEWO_HAS_POSTGRES)
        list(APPEND ECEWO_ENABLED_MODULES "postgres")
        message(STATUS "  + PostgreSQL integration module enabled")
    endif()

    list(TRANSFORM ECEWO_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

    if(ECEWO_BUILD_SHARED)
        add_library(ecewo SHARED ${ECEWO_SOURCES})
        message(STATUS "Building ecewo as SHARED library")
    else()
        add_library(ecewo STATIC ${ECEWO_SOURCES})
        message(STATUS "Building ecewo as STATIC library")
    endif()

    add_library(ecewo::ecewo ALIAS ecewo)

    if(WIN32)
        target_compile_definitions(ecewo PRIVATE PLATFORM_WINDOWS)
    elseif(APPLE)
        target_compile_definitions(ecewo PRIVATE PLATFORM_MACOS)
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(ecewo PRIVATE PLATFORM_LINUX)
    endif()

    if(ECEWO_COOKIE)
        set(ECEWO_HAS_COOKIE 1)
    endif()

    if(ECEWO_SESSION)
        set(ECEWO_HAS_SESSION 1)
    endif()

    if(ECEWO_CORS)
        set(ECEWO_HAS_CORS 1)
    endif()

    if(ECEWO_HELMET)
        set(ECEWO_HAS_HELMET 1)
    endif()

    if(ECEWO_FS)
        set(ECEWO_HAS_FS 1)
    endif()

    if(ECEWO_STATIC)
        set(ECEWO_HAS_STATIC 1)
    endif()

    if(ECEWO_POSTGRES)
        set(ECEWO_HAS_POSTGRES 1)
    endif()

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/config/ecewo-config.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/include/ecewo-config.h
        @ONLY
    )

    target_include_directories(ecewo 
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>  # Generated header
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/libuv-src/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/src/server
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
            ${CMAKE_CURRENT_SOURCE_DIR}/vendors
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            ${CMAKE_BINARY_DIR}/_deps/llhttp-src/include
    )

    if(ECEWO_BUILD_SHARED)
        target_link_libraries(ecewo PUBLIC uv llhttp_shared)
        message(STATUS "  - Linking with shared libuv and llhttp")
    else()
        target_link_libraries(ecewo PUBLIC uv_a llhttp_static)
        message(STATUS "  - Linking with static libuv and llhttp")
    endif()

    if(WIN32)
        target_link_libraries(ecewo PRIVATE wsock32 ws2_32)
    endif()

    if(MSVC)
        target_compile_options(ecewo PRIVATE 
            /W4      # High warning level
            /wd4996  # Disable deprecated function warnings
        )
        
        target_compile_definitions(ecewo PRIVATE 
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN   # Reduce Windows header bloat
        )
        
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(ecewo PRIVATE 
                /O2         # Balanced optimization
                /Oi         # Enable intrinsic functions
                /Ot         # Favor fast code
                /GL         # Whole program optimization
                /GF         # String pooling
                /Gy         # Function-level linking
            )
            
            target_link_options(ecewo PRIVATE 
                /LTCG       # Link time code generation
                /OPT:REF    # Remove unreferenced functions/data
                /OPT:ICF    # Identical COMDAT folding
            )
        endif()
    else()
        target_compile_options(ecewo PRIVATE 
            -Wall 
            -Wextra 
            -std=c99
        )
        target_compile_options(ecewo PUBLIC
            -fno-strict-aliasing    # Recommended by libuv
        )
        
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(ecewo PRIVATE 
                -O2                      # Balanced optimization
                -ffunction-sections      # Each function in separate section
                -fdata-sections          # Each data item in separate section
                -fstack-protector-strong # Stack protection
                -D_FORTIFY_SOURCE=2      # Runtime buffer overflow detection
            )
            
            target_compile_options(ecewo PRIVATE 
                -finline-functions     # Reasonable inlining
                -funroll-loops         # Loop unrolling
                -ftree-vectorize       # Auto-vectorization
            )
            
            target_link_options(ecewo PRIVATE 
                -Wl,--gc-sections       # Remove unused sections
            )
            
            if(NOT APPLE)
                target_link_options(ecewo PRIVATE -Wl,--strip-all)
            endif()
            
            message(STATUS "  - Production compiler optimizations applied")
        endif()
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT WIN32)
        include(CheckCCompilerFlag)
        check_c_compiler_flag("-fstack-protector-strong" HAS_STACK_PROTECTOR_STRONG)
        if(HAS_STACK_PROTECTOR_STRONG)
            target_compile_options(ecewo PRIVATE -fstack-protector-strong)
        else()
            target_compile_options(ecewo PRIVATE -fstack-protector)
        endif()
        
        target_compile_options(ecewo PRIVATE 
            -fPIE                   # Position independent executable
            -Wformat                # Format string vulnerabilities
            -Wformat-security       # Additional format checks
            -Werror=format-security # Make format security errors fatal
        )
        target_link_options(ecewo PRIVATE -pie)
        
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_options(ecewo PRIVATE 
                -Wl,-z,relro         # Read-only relocations
                -Wl,-z,now           # Immediate binding
            )
        endif()
        
        check_c_compiler_flag("-fcf-protection" HAS_CF_PROTECTION)
        if(HAS_CF_PROTECTION)
            target_compile_options(ecewo PRIVATE -fcf-protection)
        endif()
        
        message(STATUS "  - Security hardening enabled")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(ecewo PRIVATE
            NDEBUG
        )
    endif()

    set_target_properties(ecewo PROPERTIES
        OUTPUT_NAME ecewo
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME ecewo
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        set_property(TARGET ecewo PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    message(STATUS "=== ECEWO Build Configuration ===")
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
    if(ECEWO_BUILD_SHARED)
        message(STATUS "Library type: SHARED")
    else()
        message(STATUS "Library type: STATIC")
    endif()
    
    if(ECEWO_ENABLED_MODULES)
        message(STATUS "Modules: ${ECEWO_ENABLED_MODULES}")
    else()
        message(STATUS "Modules: None (core only)")
    endif()
    
    message(STATUS "Target system: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "Target processor: ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "LTO enabled: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
        message(STATUS "Strip binaries: ${CMAKE_STRIP_BINARIES}")
    endif()
    message(STATUS "=================================")
endif()

option(ECEWO_BUILD_TESTS "Build tests" OFF)

if(ECEWO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
