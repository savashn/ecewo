@echo off
echo ecewo v0.12.0 - Build Script  
echo 2025 (c) Savas Sahin ^<savashn^>
echo.

REM Create build directory if it doesn't exist
if not exist build mkdir build
echo Creating build directory...

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