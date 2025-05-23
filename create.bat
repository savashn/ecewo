@echo off
setlocal EnableDelayedExpansion

echo Create a project:
set /p PROJECT_NAME=Enter project name ^>^>^> 

set FILE_URL=https://raw.githubusercontent.com/savashn/ecewo-plugins/main/ecewo.bat
curl -O %FILE_URL%

for %%A in (%*) do (
    if "%%~A"=="--dev" (
        if not exist dev mkdir dev
    )
    else (
        if not exist src mkdir src
    )
)

> src\handlers.h (
    echo #ifndef HANDLERS_H
    echo #define HANDLERS_H
    echo.
    echo #include "ecewo.h"
    echo.
    echo void hello_world^(Req *req, Res *res^);
    echo.
    echo #endif
)

> src\handlers.c (
    echo #include "handlers.h"
    echo.
    echo void hello_world^(Req *req, Res *res^)
    echo {
    echo     text^(200, "hello world!"^);
    echo }
)

> src\main.c (
    echo #include "server.h"
    echo #include "handlers.h"
    echo.
    echo int main^(^)
    echo {
    echo     init_router^(^);
    echo     get^("/", hello_world^);
    echo     ecewo^(4000^);
    echo     final_router^(^);
    echo     return 0;
    echo }
)

> src\CMakeLists.txt (
    echo cmake_minimum_required^(VERSION 3.10^)
    echo project^(!PROJECT_NAME! VERSION 0.1.0 LANGUAGES C^)
    echo.
    echo set^(APP_SRC
    echo     ^${CMAKE_CURRENT_SOURCE_DIR}/main.c
    echo     ^${CMAKE_CURRENT_SOURCE_DIR}/handlers.c
    echo     PARENT_SCOPE
    echo ^)
)

echo Starter project "!PROJECT_NAME!" created successfully.

endlocal

del /f /q create.sh 2>nul
del /f /q create.bat 2>nul
