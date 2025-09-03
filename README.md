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

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.14)
project(your_project VERSION 1.0.0 LANGUAGES C)

include(FetchContent)

FetchContent_Declare(
    ecewo
    GIT_REPOSITORY https://github.com/savashn/ecewo.git
    GIT_TAG main # or use a specific version, such as v2.0.0
)

FetchContent_MakeAvailable(ecewo)

add_executable(server
    main.c
)

target_link_libraries(server PRIVATE ecewo)
```

**main.c:**
```c
#include "ecewo.h"
#include <stdlib.h>
#include <stdio.h>

void hello_world(Req *req, Res *res)
{
    send_text(res, 200, "Hello, World!");
}

int main(void)
{
    if (server_init() != SERVER_OK)
    {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }

    get("/", hello_world);

    if (server_listen(3000) != SERVER_OK)
    {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    server_run();
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

Lower is better.

| Framework  | Average   | Median   | Max     | P90      | P95     |
|------------|-----------|----------|---------|----------|---------|
| Ecewo      | 0.440ms   | 0.504ms  | 7.73ms  | 1.0ms    | 1.26ms  |
| Axum       | 0.507ms   | 0.509ms  | 10.78ms | 1.04ms   | 1.54ms  |
| Go         | 0.939ms   | 0.716ms  | 51.09ms | 1.8ms    | 2.39ms  |
| Dotnet     | 1.01ms    | 0.736ms  | 55.15ms | 2ms      | 2.74ms  |
| Express.js | 1.59ms    | 1.35ms   | 9ms     | 2.96ms   | 3.53ms  |

## Documentation

Refer to the [docs](https://ecewo.vercel.app) for usage.

## Example App

[Here](https://github.com/savashn/ecewo-example) is an example blog app built with Ecewo and PostgreSQL.

## License

Licensed under [MIT](./LICENSE).
