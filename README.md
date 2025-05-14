<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/ecewo/assets/ecewo.svg" />
    </a>
</div>

### Built for Modern Web Development in C

A modern and developer-friendly backend framework for C that handles the complexities of C programming and lets you build backends with ease — inspired by the simplicity of Express.js.

It's not production-ready yet and it doesn't need to be, because it's a **hobby project** I started to improve my programming skills. See [FAQ](https://ecewo.vercel.app/docs/faq).

### Table of Contents

- [Out of The Box Features](#out-of-the-box-features)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Example Hello World](#example-hello-world)
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

And then run the following commands to create an example starter:

For Linux/macOS:
```
chmod +x build.sh
./build.sh --create
./build.sh --run
```

For Windows PowerShell:
```
./build.bat /create
./build.bat /run
```

These commands will automatically create the following `hello world` example and run the server at `http://localhost:4000`.

### Example Hello World

```sh
// src/handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo.h"

void hello_world(Req *req, Res *res);

#endif
```

```sh
// src/handlers.c

#include "handlers.h"

void hello_world(Req *req, Res *res)
{
    reply(res, "200 OK", "text/plain", "hello world!");
}

```

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

And run `./build.sh --run` or `./build.bat /run` again.

### Documentation

Refer to the [docs](https://ecewo.vercel.app) to start building a backend with ecewo.

### License

Licensed under [MIT](./LICENSE).
