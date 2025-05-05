<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/ecewo/assets/ecewo.svg" />
    </a>
</div>

<br /><br />

ecewo is a modern, lightweight and DX-first web framework for C — built for effortless backend development with the power of C, inspired by the simplicity of Express.js.
---

### Table of Contents

- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Documentation](#documentation)
- [License](#license)

### Requirements

- CMake version 3.10 or higher
- A C compiler (GCC, Clang, or MSVC)
- MINGW64 if you are using Windows

### Quick Start

Clone this repo:

```
git clone https://github.com/savashn/ecewo.git
cd ecewo
```

Set up a `src` folder:

```
mkdir src
cd src
touch main.c handlers.c handlers.h CMakeLists.txt
```

Or, if you use PowerShell:

```
mkdir src
cd src
ni main.c
ni handlers.c
ni handlers.h
ni CMakeLists.txt
```

Write a handler:

```sh
// src/handlers.c

#include "router.h"

void hello_world(Req *req, Res *res)
{
    reply(res, "200 OK", "text/plain", "hello world!");
}

```

Declare the handler in `handlers.h`:

```sh
// src/handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "router.h"

void hello_world(Req *req, Res *res);

#endif
```

Set up the entry point:

```sh
// src/main.c

#include "server.h"
#include "router.h"
#include "handlers.h"

int main()
{
    get("/", hello_world);
    ecewo(4000);
    return 0;
}
```

Set up the `CMakeLists.txt`:

```
cmake_minimum_required(VERSION 3.10)
project(my-project VERSION 0.1.0 LANGUAGES C)

set(APP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/handlers.c
    PARENT_SCOPE
)
```

In your terminal, run `./build.sh` for Linux/macOS or `./build.bat` for Windows and go to `http://localhost:4000/`.

### Compile

For Linux/macOS:

```
# 1. Using script:

chmod +x build.sh
./build.sh
```

```
# 2. Manually:

mkdir -p build && cd build
cmake ..
cmake --build .
./server
```

For Windows:

```
# 1. Using Script:

./build.bat
```

```
# 2. Manually:

if not exist build mkdir build
cd build
cmake ..
cmake --build . --config Release
Release\server.exe
```

To rebuild from scratch:
```
// For Linux/macOS:

./build.sh --rebuild
```

```
// For Windows:

./build.bat /rebuild
```

### Documentation

Refer to the [docs](https://ecewo.vercel.app) to start building a backend with ecewo.

1. [Getting Started](https://ecewo.vercel.app/docs/getting-started)
    - 1.1 [Requirements](https://ecewo.vercel.app/docs/getting-started#requirements)
    - 1.2 [Installation](https://ecewo.vercel.app/docs/getting-started#installation)
    - 1.3 [Start Server](https://ecewo.vercel.app/docs/getting-started#start-server)
        - 1.4.1 [Write The Entry Point](https://ecewo.vercel.app/docs/getting-started#write-the-entry-point)
        - 1.4.2 [Build And Run The Server](https://ecewo.vercel.app/docs/getting-started#build-and-run-the-server)
2. [Route Handling](https://ecewo.vercel.app/docs/route-handling)
    - 2.1 [Handlers](https://ecewo.vercel.app/docs/route-handling#handlers)
    - 2.2 [Declaring Routes](https://ecewo.vercel.app/docs/route-handling#declaring-routes)
    - 2.3 [Notes](https://ecewo.vercel.app/docs/route-handling#notes)
3. [Handling Requests](https://ecewo.vercel.app/docs/handling-requests)
    - 3.1 [Request Body](https://ecewo.vercel.app/docs/handling-requests#request-body)
    - 3.2 [Request Params](https://ecewo.vercel.app/docs/handling-requests#request-params)
    - 3.3 [Request Query](https://ecewo.vercel.app/docs/handling-requests#request-query)
    - 3.4 [Request Headers](https://ecewo.vercel.app/docs/handling-requests#request-headers)
4. [Using JSON](https://ecewo.vercel.app/docs/using-json)
    - 4.1 [Creating JSON](https://ecewo.vercel.app/docs/using-json#creating-json)
    - 4.2 [Parsing JSON](https://ecewo.vercel.app/docs/using-json#parsing-json)
5. [Using A Database](https://ecewo.vercel.app/docs/using-a-database)
    - 5.1 [Install SQLite](https://ecewo.vercel.app/docs/using-a-database#install-sqlite)
    - 5.2 [Example Folder Structure](https://ecewo.vercel.app/docs/using-a-database#example-folder-structure)
    - 5.3 [Change The Makefile](https://ecewo.vercel.app/docs/using-a-database#change-the-makefile)
    - 5.4 [Connecting To Database](https://ecewo.vercel.app/docs/using-a-database#connecting-to-database)
    - 5.5 [Example Usage](https://ecewo.vercel.app/docs/using-a-database#example-usage)
        - 5.5.1 [Inserting Data](https://ecewo.vercel.app/docs/using-a-database#inserting-data)
        - 5.5.2 [Querying Data](https://ecewo.vercel.app/docs/using-a-database#querying-data)
6. [Authentication](https://ecewo.vercel.app/docs/authentication)
    - 6.1 [Login](https://ecewo.vercel.app/docs/authentication#login)
    - 6.2 [Logout](https://ecewo.vercel.app/docs/authentication#logout)
    - 6.3 [Getting Session Data](https://ecewo.vercel.app/docs/authentication#getting-session-data)
    - 6.4 [Protected Routes](https://ecewo.vercel.app/docs/authentication#protected-routes)
    - 6.5 [Notes](https://ecewo.vercel.app/docs/authentication#notes)
7. [Middleware](https://ecewo.vercel.app/docs/middleware)
    - 7.1 [Route Middleware](https://ecewo.vercel.app/docs/middleware#route-middleware)
    - 7.2 [Global Middleware](https://ecewo.vercel.app/docs/middleware#global-middleware)
8. [Environment Variables](https://ecewo.vercel.app/docs/environment-variables)

### License

Licensed under [MIT](./LICENSE).
