#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Unicode symbols
CHECK="\u2713"
CROSS="\u2717"
ARROW="\u2192"

echo -e "${CYAN}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║           ECEWO Framework Test Runner              ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════╝${NC}"

# Function to run test and check result
run_test() {
    local test_name=$1
    local test_command=$2
    local optional=${3:-false}
    
    echo -e "\n${PURPLE}${ARROW} Running $test_name...${NC}"
    echo "Command: $test_command"
    echo "----------------------------------------"
    
    if eval $test_command; then
        if [ "$optional" = true ]; then
            echo -e "${GREEN}${CHECK} $test_name COMPLETED${NC}"
        else
            echo -e "${GREEN}${CHECK} $test_name PASSED${NC}"
        fi
        return 0
    else
        if [ "$optional" = true ]; then
            echo -e "${YELLOW}${CROSS} $test_name FAILED (OPTIONAL)${NC}"
            return 0  # Don't fail the overall script for optional tests
        else
            echo -e "${RED}${CROSS} $test_name FAILED${NC}"
            return 1
        fi
    fi
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check prerequisites
echo -e "${BLUE}Checking prerequisites...${NC}"

if ! command_exists cmake; then
    echo -e "${RED}${CROSS} CMake not found. Please install CMake.${NC}"
    exit 1
fi

# Detect build system
BUILD_SYSTEM=""
CMAKE_GENERATOR=""

# MSYS2/Windows environment often defaults to Ninja
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    if command_exists ninja; then
        BUILD_SYSTEM="ninja"
        CMAKE_GENERATOR="-G Ninja"
    elif command_exists mingw32-make; then
        BUILD_SYSTEM="mingw32-make"
        CMAKE_GENERATOR="-G \"MinGW Makefiles\""
    else
        echo -e "${RED}${CROSS} No build system found. Please install ninja or mingw32-make.${NC}"
        exit 1
    fi
else
    # Unix-like systems
    if command_exists make; then
        BUILD_SYSTEM="make"
        CMAKE_GENERATOR=""
    elif command_exists ninja; then
        BUILD_SYSTEM="ninja"
        CMAKE_GENERATOR="-G Ninja"
    else
        echo -e "${RED}${CROSS} No build system found. Please install make or ninja.${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}${CHECK} Prerequisites satisfied (using $BUILD_SYSTEM)${NC}"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}${CROSS} CMakeLists.txt not found. Please run from tests directory.${NC}"
    exit 1
fi

# Build project
echo -e "\n${BLUE}Building project...${NC}"
mkdir -p build && cd build

# Configure CMake with appropriate generator
if [ -n "$CMAKE_GENERATOR" ]; then
    eval "cmake $CMAKE_GENERATOR -DCMAKE_BUILD_TYPE=Debug .."
else
    cmake -DCMAKE_BUILD_TYPE=Debug ..
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}${CROSS} CMake configuration failed!${NC}"
    exit 1
fi

# Auto-detect build system from generated files if not explicitly set
if [ -f "build.ninja" ]; then
    BUILD_SYSTEM="ninja"
    echo -e "${BLUE}Detected Ninja build files${NC}"
elif [ -f "Makefile" ]; then
    if command_exists mingw32-make; then
        BUILD_SYSTEM="mingw32-make"
    else
        BUILD_SYSTEM="make"
    fi
    echo -e "${BLUE}Detected Makefile build files${NC}"
fi

CPU_COUNT=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Build with appropriate build system
case "$BUILD_SYSTEM" in
    "ninja")
        ninja
        ;;
    "mingw32-make")
        mingw32-make -j$CPU_COUNT
        ;;
    "make")
        make -j$CPU_COUNT
        ;;
    *)
        echo -e "${RED}${CROSS} Unknown build system: $BUILD_SYSTEM${NC}"
        exit 1
        ;;
esac

if [ $? -ne 0 ]; then
    echo -e "${RED}${CROSS} Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}${CHECK} Build successful${NC}"

# Track test results
failed_tests=0
total_tests=0

# Core tests (required)
echo -e "\n${CYAN}═══════════════════════════════════════════════════${NC}"
echo -e "${CYAN}                    CORE TESTS                     ${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════${NC}"

# Detect executable suffix for Windows
EXE_SUFFIX=""
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    EXE_SUFFIX=".exe"
fi

# Try minimal test first
((total_tests++))
if [ -f "./minimal_test$EXE_SUFFIX" ]; then
    if run_test "Minimal Tests" "./minimal_test$EXE_SUFFIX"; then
        echo
    else
        ((failed_tests++))
    fi
else
    echo -e "${YELLOW}${CROSS} Minimal tests not built (SKIPPED)${NC}"
fi

# Try main test suite
((total_tests++))
if [ -f "./ecewo_tests$EXE_SUFFIX" ]; then
    if run_test "Unit Tests" "./ecewo_tests$EXE_SUFFIX"; then
        echo
    else
        ((failed_tests++))
    fi
else
    echo -e "${RED}${CROSS} Main test suite not built${NC}"
    ((failed_tests++))
fi

# Performance tests (optional but recommended)
echo -e "\n${CYAN}═══════════════════════════════════════════════════${NC}"
echo -e "${CYAN}                PERFORMANCE TESTS                  ${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════${NC}"

((total_tests++))
if [ -f "./ecewo_perf_tests$EXE_SUFFIX" ]; then
    run_test "Performance Tests" "./ecewo_perf_tests$EXE_SUFFIX" true
else
    echo -e "${YELLOW}${CROSS} Performance tests not built (SKIPPED)${NC}"
fi

# Memory tests (optional, requires valgrind - Linux/macOS only)
echo -e "\n${CYAN}═══════════════════════════════════════════════════${NC}"
echo -e "${CYAN}                  MEMORY TESTS                     ${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════${NC}"

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    echo -e "${YELLOW}${CROSS} Memory tests not supported on Windows (SKIPPED)${NC}"
    echo -e "${YELLOW}    Use WSL or Linux/macOS for valgrind memory testing${NC}"
elif command_exists valgrind; then
    ((total_tests++))
    if [ "$BUILD_SYSTEM" = "ninja" ]; then
        run_test "Memory Tests (Valgrind)" "valgrind --tool=memcheck --leak-check=full --error-exitcode=1 ./ecewo_tests$EXE_SUFFIX" true
    else
        run_test "Memory Tests (Valgrind)" "make memory_tests" true
    fi
else
    echo -e "${YELLOW}${CROSS} Valgrind not found (SKIPPED)${NC}"
    echo -e "${YELLOW}    Install valgrind for memory leak detection${NC}"
fi

# Static analysis (optional, requires cppcheck)
echo -e "\n${CYAN}═══════════════════════════════════════════════════${NC}"
echo -e "${CYAN}                 STATIC ANALYSIS                   ${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════${NC}"

if command_exists cppcheck; then
    ((total_tests++))
    run_test "Static Analysis" "cppcheck --enable=all --error-exitcode=1 ../src/" true
else
    echo -e "${YELLOW}${CROSS} cppcheck not found (SKIPPED)${NC}"
    echo -e "${YELLOW}    Install cppcheck for static analysis${NC}"
fi

# Coverage report (optional, requires lcov - Linux/macOS only)
echo -e "\n${CYAN}═══════════════════════════════════════════════════${NC}"
echo -e "${CYAN}                COVERAGE ANALYSIS                  ${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════${NC}"

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    echo -e "${YELLOW}${CROSS} Coverage analysis not fully supported on Windows (SKIPPED)${NC}"
    echo -e "${YELLOW}    Use WSL or Linux/macOS for lcov coverage reports${NC}"
elif command_exists lcov; then
    echo -e "${BLUE}${ARROW} Generating coverage report...${NC}"
    if [ "$BUILD_SYSTEM" = "ninja" ]; then
        echo -e "${YELLOW}${CROSS} Coverage with ninja not implemented (SKIPPED)${NC}"
    elif make coverage >/dev/null 2>&1; then
        echo -e "${GREEN}${CHECK} Coverage report generated in coverage_html/${NC}"
        echo -e "${BLUE}    Open coverage_html/index.html to view detailed report${NC}"
    else
        echo -e "${YELLOW}${CROSS} Coverage report generation failed${NC}"
    fi
else
    echo -e "${YELLOW}${CROSS} lcov not found (SKIPPED)${NC}"
    echo -e "${YELLOW}    Install lcov and genhtml for coverage reports${NC}"
fi

# Summary
echo -e "\n${CYAN}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                   TEST SUMMARY                     ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════╝${NC}"

echo -e "Total Test Suites: $total_tests"
echo -e "Failed Test Suites: $failed_tests"

if [ $failed_tests -eq 0 ]; then
    success_rate="100%"
    echo -e "Success Rate: ${GREEN}$success_rate${NC}"
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  🎉 ALL CRITICAL TESTS PASSED! SHIP IT! 🚀        ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════╝${NC}"
    
    # Additional success info
    echo -e "\n${BLUE}Next steps:${NC}"
    echo -e "  • Review performance metrics above"
    echo -e "  • Check coverage report if generated"
    echo -e "  • Run integration tests in staging environment"
    echo -e "  • Consider running stress tests for production readiness"
    
    exit 0
else
    success_rate=$(( (total_tests - failed_tests) * 100 / total_tests ))
    echo -e "Success Rate: ${YELLOW}$success_rate%${NC}"
    echo ""
    echo -e "${RED}╔════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  ❌ $failed_tests TEST SUITE(S) FAILED!                      ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════╝${NC}"
    
    # Failure guidance
    echo -e "\n${RED}Debugging tips:${NC}"
    echo -e "  • Check error messages above for specific failures"
    echo -e "  • Run individual tests with: ./ecewo_tests"
    echo -e "  • Use valgrind for memory issues: valgrind ./ecewo_tests"
    echo -e "  • Enable debug symbols: cmake -DCMAKE_BUILD_TYPE=Debug"
    echo -e "  • Check build logs for compilation warnings"
    
    exit 1
fi