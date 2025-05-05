#!/bin/bash
# ecewo build script for Unix systems

# For colored output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}ecewo - Build Script${NC}"
echo -e "${BLUE}2025 © Savas Sahin <savashn>${NC}"
echo ""

# Check if --rebuild parameter is provided
if [ "$1" == "--rebuild" ]; then
    echo -e "${YELLOW}Full rebuild requested. Removing build directory...${NC}"
    rm -rf build
    echo -e "${GREEN}Creating fresh build directory...${NC}"
    mkdir build
else
    # Create build directory if it doesn't exist
    if [ ! -d "build" ]; then
        echo -e "${GREEN}Creating build directory...${NC}"
        mkdir build
    fi
fi

# Configure and build with cmake
cd build
echo -e "${GREEN}Configuring with CMake...${NC}"
cmake ..

echo -e "${GREEN}Building...${NC}"
cmake --build .

echo -e "${GREEN}Build completed!${NC}"
echo ""

# Run
echo -e "${BLUE}Running ecewo server...${NC}"
if [ -f "./server" ]; then
    ./server
else
    echo -e "${YELLOW}Server executable not found. Check for build errors.${NC}"
fi

# Return to the starting directory
cd ..
