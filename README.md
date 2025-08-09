<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/assets/ecewo.svg" />
    </a>
</div>

<hr />

## Express-C Effect for Web Operations

Inspired by [Express.js](https://expressjs.com/). It’s minimalist, unopinionated, and easy-to-use (in a C kind of way).

<hr />

> **This is a hobby project that I'm developing to improve my programming skills. So it might not be production-ready, and it doesn't have to be.**

## Table of Contents

- [Quick Start](#quick-start)
- [Out of The Box Features](#out-of-the-box-features)
- [Requirements](#requirements)
- [Benchmarks](#benchmarks)
- [Documentation](#documentation)
- [Example App](#example-app)
- [License](#license)

## Quick Start

### Using [Ecewo-CLI:](https://github.com/savashn/ecewo-cli)

```
ecewo create
```

### Manually:

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.14)
project(your_project VERSION 1.0.0 LANGUAGES C)

include(FetchContent)

FetchContent_Declare(
    ecewo
    GIT_REPOSITORY https://github.com/savashn/ecewo.git
    GIT_TAG main
)

FetchContent_MakeAvailable(ecewo)

add_executable(server
    main.c
)

target_link_libraries(server PRIVATE ecewo)
```

**main.c:**
```c
#include "server.h"  // To start and end the server
#include "ecewo.h"   // To use the main API

void hello_world(Req *req, Res *res)
{
    send_text(res, 200, "Hello, World!");
}

void destroy_app(void)
{
    reset_router();
}

int main(void)
{
    init_router();
    get("/", hello_world);
    shutdown_hook(destroy_app);
    ecewo(3000);
    return 0;
}
```

**Build:**

```shell
mkdir build && cd build && cmake .. && cmake --build .
```

## Out of The Box Features

- Full asynchronous operations support
- Cross-platform compatibility
- JSON and CBOR support
- Cookie management and optional session based authentication mechanism
- Flexible middleware support (route-specific and global)
- Express.js-like routing mechanism

## Requirements

- A C compiler
- CMake version 3.14 or higher

## Benchmarks

Here are 'Hello World' benchmark results for several frameworks compared to Ecewo. See the source code of the [benchmark test](https://github.com/savashn/ecewo-benchmarks).

| Framework | Average   | Median   | Max     | P90      | P95     |
|-----------|-----------|----------|---------|----------|---------|
| Ecewo     | 166.24µs  | 0s       | 19.52ms | 545.29µs | 922µs   |
| Axum      | 185.67µs  | 0s       | 35.9ms  | 549.79µs | 971.5µs |
| Go        | 720.42µs  | 598.19µs | 22.15ms | 1.46ms   | 1.93ms  |
| Hono      | 393.05µs  | 341.8µs  | 23.08ms | 1ms      | 1.12ms  |
| Express   | 1.38ms    | 1.08ms   | 18.83ms | 2.83ms   | 3.7ms   |

## Documentation

Refer to the [docs](https://ecewo.vercel.app) for usage.

## Example App

[Here](https://github.com/savashn/ecewo-example) is an example blog app built with Ecewo and PostgreSQL.

## License

Licensed under [MIT](./LICENSE).
