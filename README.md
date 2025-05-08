<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/ecewo/assets/ecewo.svg" />
    </a>
</div>

### Built for Modern Web Development in C

A modern and developer-friendly backend framework for C that handles the complexities of C programming and lets you build backends with ease — inspired by the simplicity of Express.js.

### Table of Contents

- [Out of The Box Features](#out-of-the-box-features)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Documentation](#documentation)
- [License](#license)

### Out of The Box Features

- Full asynchronous operations support
- Built-in JSON parsing and generation
- Session-based authentication mechanism
- Easy management of environment variables
- Flexible middleware support (route-specific and global)
- Express.js-like routing mechanism

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

#include "ecewo.h"
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

### License

Licensed under [MIT](./LICENSE).
