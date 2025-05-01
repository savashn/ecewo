#!/bin/bash
# ecewo v0.12.0 build script for Unix systems

# For colored output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}ecewo v0.12.0 - Build Script${NC}"
echo -e "${BLUE}2025 © Savas Sahin <savashn>${NC}"
echo ""

# Create or clean the build directory
if [ ! -d "build" ]; then
    echo -e "${GREEN}Creating build directory...${NC}"
    mkdir build
else
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build/*
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
