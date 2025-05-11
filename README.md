<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/ecewo/assets/ecewo.svg" />
    </a>
</div>

### Built for Modern Web Development in C

A modern and developer-friendly backend framework for C that handles the complexities of C programming and lets you build backends with ease — inspired by the simplicity of Express.js.

It's a **hobby project** I started to improve my programming skills. See [FAQ](https://ecewo.vercel.app/docs/faq).

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

Declare a handler in `handlers.h`:

```sh
// src/handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo.h"

void hello_world(Req *req, Res *res);

#endif
```

Write the handler:

```sh
// src/handlers.c

#include "handlers.h"

void hello_world(Req *req, Res *res)
{
    reply(res, "200 OK", "text/plain", "hello world!");
}

```

Set up the entry point:

```sh
// src/main.c

#include "server.h"
#include "handlers.h"

int main()
{
    init_router();
    get("/", hello_world);
    ecewo(4000);
    free_router();
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
chmod +x build.sh
./build.sh
```

For Windows:

```
./build.bat
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

<--- **NOTE** --->

If you have the following issue while compiling:
```
CMake Error at build/_deps/jansson-src/CMakeLists.txt:1 (cmake_minimum_required):
  Compatibility with CMake < 3.5 has been removed from CMake.
```

Go to `build/_deps/jansson-src/` and modify the `CMakeLists.txt` as follows:
```
// Change this:
cmake_minimum_required (VERSION 3.1)
project(jansson C)

// To this:
cmake_minimum_required (VERSION 3.10)
project(jansson C)
```

And run the build command again.

### Documentation

Refer to the [docs](https://ecewo.vercel.app) to start building a backend with ecewo.

### License

Licensed under [MIT](./LICENSE).
