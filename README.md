<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/assets/ecewo.svg" />
    </a>
</div>

<hr />

### Minimalist and easy-to-use web framework for C — inspired by the simplicity of [Express.js](https://expressjs.com/).

**This is a hobby project that I'm developing to improve my programming skills. So it's not stable or production-ready. See [FAQ](https://ecewo.vercel.app/docs/faq).**

<hr />

### Table of Contents

- [Out of The Box Features](#out-of-the-box-features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Example "Hello World"](#example-hello-world)
- [Documentation](#documentation)
- [License](#license)

### Out of The Box Features

- Full asynchronous operations support
- JSON and CBOR support
- Cookie management and optional session based authentication mechanism
- Flexible middleware support (route-specific and global)
- Express.js-like routing mechanism

### Requirements

- A C compiler (GCC, Clang, or MSVC)
- CMake version 3.10 or higher

### Installation

Add Ecewo into your `CMakeLists.txt`:
```cmake
include(FetchContent)

FetchContent_Declare(
    ecewo
    GIT_REPOSITORY https://github.com/savashn/ecewo.git
    GIT_TAG main
)

FetchContent_MakeAvailable(ecewo)
```

And link Ecewo:
```cmake
target_link_libraries(your_project PRIVATE ecewo)
```

Build:
```shell
mkdir build && cd build
cmake .. && cmake --build .
```

### Example "Hello World"

```c
// src/main.c

#include "server.h"    // To start the server via ecewo();
#include "ecewo.h"     // To use the API

void hello_world(Req *req, Res *res)
{
    send_text(200, "Hello, World!");
}

int main()
{
    init_router();
    get("/", hello_world);
    ecewo(4000);
    reset_router();
    return 0;
}
```

### Documentation

Refer to the [docs](https://ecewo.vercel.app) for usage.

### License

Licensed under [MIT](./LICENSE).
