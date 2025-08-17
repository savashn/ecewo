cmake_minimum_required(VERSION 3.14)
project(ecewo_tests VERSION 1.0.0 LANGUAGES C)

# Include parent directory for ecewo library
set(ECEWO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${ECEWO_ROOT}/src)
include_directories(${ECEWO_ROOT}/src/lib)
include_directories(${ECEWO_ROOT}/src/server)
include_directories(${ECEWO_ROOT}/src/utils)
include_directories(${ECEWO_ROOT}/src/config)

# Add the parent project
add_subdirectory(${ECEWO_ROOT} ecewo_lib)

# Test sources
set(TEST_SOURCES
    test_main.c
    unit_tests.c
    mock_helpers.c
)

# Create main test executable
add_executable(ecewo_tests ${TEST_SOURCES})

# Link with ecewo library
target_link_libraries(ecewo_tests ecewo)

# Compiler settings for tests
if(MSVC)
    target_compile_options(ecewo_tests PRIVATE /W4)
    target_compile_definitions(ecewo_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(ecewo_tests PRIVATE -Wall -Wextra -std=gnu11 -g)
endif()

# Add custom target to run minimal tests
add_custom_target(run_minimal_tests
    COMMAND minimal_test
    DEPENDS minimal_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running minimal tests"
)

# Add custom target to run tests
add_custom_target(run_tests
    COMMAND ecewo_tests
    DEPENDS ecewo_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ecewo tests"
)

# Performance tests executable
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/performance_tests.c)
    add_executable(ecewo_perf_tests performance_tests.c)
    target_link_libraries(ecewo_perf_tests ecewo)
    
    if(NOT MSVC)
        target_compile_options(ecewo_perf_tests PRIVATE -Wall -Wextra -std=gnu11 -O2)
    endif()
endif()

# Memory tests (with valgrind on Linux)
if(UNIX AND NOT APPLE)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        add_custom_target(memory_tests
            COMMAND ${VALGRIND_EXECUTABLE} 
                --tool=memcheck 
                --leak-check=full 
                --show-leak-kinds=all 
                --track-origins=yes
                --error-exitcode=1
                $<TARGET_FILE:ecewo_tests>
            DEPENDS ecewo_tests
            COMMENT "Running memory tests with valgrind"
        )
    endif()
endif()

# Coverage tests (with gcov/lcov)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
    if(ENABLE_COVERAGE)
        target_compile_options(ecewo_tests PRIVATE --coverage)
        target_link_libraries(ecewo_tests --coverage)
        
        find_program(LCOV_EXECUTABLE lcov)
        find_program(GENHTML_EXECUTABLE genhtml)
        
        if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
            add_custom_target(coverage
                COMMAND ${LCOV_EXECUTABLE} --directory . --zerocounters
                COMMAND $<TARGET_FILE:ecewo_tests>
                COMMAND ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info
                COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' --output-file coverage.info
                COMMAND ${GENHTML_EXECUTABLE} -o coverage_html coverage.info
                DEPENDS ecewo_tests
                COMMENT "Generating code coverage report"
            )
        endif()
    endif()
endif()