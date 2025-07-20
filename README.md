<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/assets/ecewo.svg" />
    </a>
</div>

<hr />

### Minimalist and easy-to-use web framework for C — inspired by the simplicity of [Express.js](https://expressjs.com/).

So it’s really simple, but in a C kind of way.

<hr />

> **This is a hobby project that I'm developing to improve my programming skills. So it might not be production-ready yet, and it doesn't have to be. See [FAQ](https://ecewo.vercel.app/docs/faq).**

### Table of Contents

- [Out of The Box Features](#out-of-the-box-features)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Benchmarks](#benchmarks)
- [Documentation](#documentation)
- [Example App](#example-app)
- [License](#license)

### Out of The Box Features

- Full asynchronous operations support
- Cross-platform compatibility
- JSON and CBOR support
- Cookie management and optional session based authentication mechanism
- Flexible middleware support (route-specific and global)
- Express.js-like routing mechanism

### Requirements

- A C compiler (GCC, Clang, or MSVC)
- CMake version 3.14 or higher

### Quick Start

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

void hello_world(Req *req, Res *res) {
    send_text(res, 200, "Hello, World!");
}

void destroy_app() {
    reset_router();
}

int main() {
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

### Benchmarks

Here are 'Hello World' benchmark results for several frameworks compared to Ecewo. See the source code of the [benchmark test](https://github.com/savashn/ecewo-benchmarks).

| Framework | Average   | Median   | Max     | P90     | P95      |
|-----------|-----------|----------|---------|---------|----------|
| Ecewo     | 71.43µs   | 0s       | 4.97ms  | 503.4µs | 511.1µs  |
| Axum      | 98.35µs   | 0s       | 3.52ms  | 508µs   | 526.9µs  |
| Go        | 366.14µs  | 504.4µs  | 4.51ms  | 726µs   | 862.77µs |
| Hono      | 364.72µs  | 504.9µs  | 4.5ms   | 755.3µs | 1ms      |
| Fastify   | 490.55µs  | 525.5µs  | 5.99ms  | 896.2µs | 1.06ms   |
| Express   | 1.11ms    | 1.05ms   | 5.98ms  | 2ms     | 2.36ms   |

### Documentation

Refer to the [docs](https://ecewo.vercel.app) for usage.

### Example App

[Here](https://github.com/savashn/ecewo-example) is an example blog app built with Ecewo and PostgreSQL.

### License

Licensed under [MIT](./LICENSE).