@echo off
echo ecewo - Build Script  
echo 2025 (c) Savas Sahin ^<savashn^>
echo.

REM If the build directory exists, delete it
if exist build (
    echo Cleaning build directory...
    rmdir /s /q build
)

REM Create build directory
echo Creating build directory...
mkdir build

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