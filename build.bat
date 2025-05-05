@echo off
echo ecewo - Build Script for Windows
echo 2025 (c) Savas Sahin ^<savashn^>
echo.

REM Check if /rebuild parameter is provided to clean the build directory
if /i "%1"=="/rebuild" (
    echo Cleaning build directory...
    if exist build rmdir /s /q build
    echo Build directory cleaned.
    echo.
)

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Run CMake configuration
cd build
echo Configuring with CMake...  
cmake -G "Visual Studio 17 2022" -A x64 ..

REM Build the project
echo Building...
cmake --build . --config Release

echo Build completed!
echo.
echo Running ecewo server...
cd Release
if exist server.exe (
    server.exe
) else (
    echo Server executable not found. Check for build errors.
)

REM Return to original directory
cd ..\..
