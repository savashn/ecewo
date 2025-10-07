cmake_minimum_required(VERSION 3.14)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)  # DOWNLOAD_EXTRACT_TIMESTAMP
endif()
if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)  # Don't add /W3 by default
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

# Options - Only set if not already defined (FetchContent compatibility)
if(NOT DEFINED ECEWO_BUILD_SHARED)
    option(ECEWO_BUILD_SHARED "Build shared library instead of static" OFF)
endif()

# Production Optimizations Setup
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Applying production optimizations for web framework...")
    
    # Enable Link Time Optimization if supported
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "  - Link Time Optimization (LTO) enabled")
    else()
        message(WARNING "LTO not supported: ${ipo_error}")
    endif()
    
    # Strip binaries in release mode (not on Windows with MSVC)
    if(NOT (WIN32 AND MSVC))
        set(CMAKE_STRIP_BINARIES ON)
        message(STATUS "  - Binary stripping enabled")
    endif()
endif()

# Dependencies
include(FetchContent)

# Fetch libuv
FetchContent_Declare(
    libuv
    GIT_REPOSITORY https://github.com/libuv/libuv.git
    GIT_TAG v1.51.0
    GIT_SHALLOW TRUE
)

# Configure libuv to match ecewo's library type
if(ECEWO_BUILD_SHARED)
    set(LIBUV_BUILD_SHARED ON CACHE BOOL "Build shared libuv for shared ecewo" FORCE)
    message(STATUS "  - Configuring libuv as SHARED library")
else()
    set(LIBUV_BUILD_SHARED OFF CACHE BOOL "Build static libuv for static ecewo" FORCE)
    message(STATUS "  - Configuring libuv as STATIC library")
endif()

FetchContent_MakeAvailable(libuv)

# Fetch llhttp
FetchContent_Declare(
    llhttp
    URL "https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.3.0.tar.gz"
)

if(ECEWO_BUILD_SHARED)
    set(LLHTTP_BUILD_SHARED_LIBS ON CACHE INTERNAL "" FORCE)
    set(LLHTTP_BUILD_STATIC_LIBS OFF CACHE INTERNAL "" FORCE)
    set(BUILD_SHARED_LIBS ON CACHE INTERNAL "" FORCE)
    set(BUILD_STATIC_LIBS OFF CACHE INTERNAL "" FORCE)
    message(STATUS "  - Configuring llhttp as SHARED library")
else()
    set(LLHTTP_BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
    set(LLHTTP_BUILD_STATIC_LIBS ON CACHE INTERNAL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE INTERNAL "" FORCE)
    message(STATUS "  - Configuring llhttp as STATIC library")
endif()

FetchContent_MakeAvailable(llhttp)

# Prevent multiple inclusion when used as a subdirectory
if(NOT TARGET ecewo::ecewo)

    # Source Files
    set(ECEWO_SRC_FILES
        src/server/server.c
        src/lib/request.c
        src/lib/router.c
        src/lib/middleware.c
        src/lib/route_trie.c
        src/utils/task.c
        vendors/arena.c
    )

    # Make source files relative to CMAKE_CURRENT_SOURCE_DIR
    list(TRANSFORM ECEWO_SRC_FILES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

    # Create Library (Static or Shared based on option)
    if(ECEWO_BUILD_SHARED)
        add_library(ecewo SHARED ${ECEWO_SRC_FILES})
        message(STATUS "Building ecewo as SHARED library")
    else()
        add_library(ecewo STATIC ${ECEWO_SRC_FILES})
        message(STATUS "Building ecewo as STATIC library")
    endif()

    # Create alias for namespace consistency
    add_library(ecewo::ecewo ALIAS ecewo)

    # Platform-specific definitions (target-specific, not global)
    if(WIN32)
        target_compile_definitions(ecewo PRIVATE PLATFORM_WINDOWS)
    elseif(APPLE)
        target_compile_definitions(ecewo PRIVATE PLATFORM_MACOS)
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(ecewo PRIVATE PLATFORM_LINUX)
    endif()

    # Include Directories
    target_include_directories(ecewo PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendors>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/libuv-src/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/llhttp-src/include>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/ecewo>
    )

    # Link Libraries - Match ecewo's library type
    if(ECEWO_BUILD_SHARED)
        # Shared ecewo uses shared dependencies
        target_link_libraries(ecewo PUBLIC uv llhttp_shared)
        message(STATUS "  - Linking with shared libuv and llhttp")
    else()
        # Static ecewo uses static dependencies
        target_link_libraries(ecewo PUBLIC uv_a llhttp_static)
        message(STATUS "  - Linking with static libuv and llhttp")
    endif()

    # Platform-specific Linking
    if(WIN32)
        target_link_libraries(ecewo PRIVATE wsock32 ws2_32)
    endif()

    # Compiler-specific Settings
    if(MSVC)
        # Visual Studio Settings
        target_compile_options(ecewo PRIVATE 
            /W4      # High warning level
            /wd4996  # Disable deprecated function warnings
        )
        
        target_compile_definitions(ecewo PRIVATE 
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN   # Reduce Windows header bloat
        )
        
        # MSVC Production Optimizations
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
        # GCC/Clang Settings
        target_compile_options(ecewo PRIVATE 
            -Wall 
            -Wextra 
            -std=c99
        )

        target_compile_options(ecewo PUBLIC
            -fno-strict-aliasing    # Recommended by libuv
        )
        
        # GCC/Clang Production Optimizations
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(ecewo PRIVATE 
                -O2                      # Balanced optimization
                -ffunction-sections      # Each function in separate section
                -fdata-sections          # Each data item in separate section
                -fstack-protector-strong # Stack protection
                -D_FORTIFY_SOURCE=2      # Runtime buffer overflow detection
            )
            
            # Web Framework Specific Optimizations
            target_compile_options(ecewo PRIVATE 
                -finline-functions     # Reasonable inlining
                -funroll-loops         # Loop unrolling
                -ftree-vectorize       # Auto-vectorization
            )
            
            # Linker Optimizations
            target_link_options(ecewo PRIVATE 
                -Wl,--gc-sections       # Remove unused sections
            )
            
            # Platform-specific linker optimizations
            if(NOT APPLE)
                target_link_options(ecewo PRIVATE -Wl,--strip-all)
            endif()
            
            message(STATUS "  - Production compiler optimizations applied")
        endif()
    endif()

    # Security Hardening (Unix-like systems)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT WIN32)
        # Stack canary check
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
        
        # Linux-specific security features
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_options(ecewo PRIVATE 
                -Wl,-z,relro         # Read-only relocations
                -Wl,-z,now           # Immediate binding
            )
        endif()
        
        # Control Flow Integrity if supported
        check_c_compiler_flag("-fcf-protection" HAS_CF_PROTECTION)
        if(HAS_CF_PROTECTION)
            target_compile_options(ecewo PRIVATE -fcf-protection)
        endif()
        
        message(STATUS "  - Security hardening enabled")
    endif()

    # Memory Optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(ecewo PRIVATE
            NDEBUG                   # Disable assertions
            ECEWO_OPTIMIZE_FOR_SPEED=1
            ECEWO_INLINE_HOT_PATHS=1
            ECEWO_CACHE_LINE_SIZE=64  # Modern CPU cache line size
        )
    endif()

    # Target Properties
    set_target_properties(ecewo PROPERTIES
        OUTPUT_NAME ecewo
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME ecewo
    )
    
    # Apply LTO to the library if enabled
    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        set_property(TARGET ecewo PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

endif()

# Build Configuration Summary
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    message(STATUS "=== ECEWO Build Configuration ===")
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
    if(ECEWO_BUILD_SHARED)
        message(STATUS "Library type: SHARED")
    else()
        message(STATUS "Library type: STATIC")
    endif()
    message(STATUS "Target system: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "Target processor: ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "LTO enabled: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
        message(STATUS "Strip binaries: ${CMAKE_STRIP_BINARIES}")
    endif()
    message(STATUS "===============================================")
endif()
