<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/assets/ecewo.svg" />
    </a>
</div>

<hr />

### Minimalist and easy-to-use web framework for C — inspired by the simplicity of [Express.js](https://expressjs.com/).

So it’s really simple, but in a C kind of way.

<hr />

> **This is a hobby project that I'm developing to improve my programming skills. So it might not be production-ready, and it doesn't need to be. Personally, I wouldn't recommend using C for production-level web development, as it's too risky and unnecessary. See [FAQ](https://ecewo.vercel.app/docs/faq).**

### Table of Contents

- [Out of The Box Features](#out-of-the-box-features)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Benchmarks](#benchmarks)
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
- CMake version 3.14 or higher

### Quick Start

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.14)
project(your_project VERSION 1.0.0 LANGUAGES C)

include(FetchContent)

# Fetch Ecewo from GitHub
FetchContent_Declare(
    ecewo
    GIT_REPOSITORY https://github.com/savashn/ecewo.git
    GIT_TAG main
)

# Make Ecewo available
FetchContent_MakeAvailable(ecewo)

# Create the executable
add_executable(server
    main.c
)

# Link Ecewo
target_link_libraries(server PRIVATE ecewo)
```

**main.c:**
```c
#include "server.h"    // To start the server via ecewo();
#include "ecewo.h"     // To use the main API

void hello_world(Req *req, Res *res) {
    send_text(200, "Hello, World!");
}

int main() {
    init_router();
    get("/", hello_world);
    ecewo(3000);
    reset_router();
    return 0;
}
```

**Build:**

```shell
mkdir build && cd build && cmake .. && cmake --build .
```

### Benchmarks

Here are 'Hello World' benchmark results for several frameworks compared to Ecewo. See the source code of the [benchmark test](https://github.com/savashn/ecewo-benchmarks).

| Framework | Latency.Avg | Latency.Max | Request.Total | Request.Req/Sec | Transfer.Total | Transfer.Rate |
|---|---|---|---|---|---|---|
|ecewo|62.53µs|3.89ms|11,987|66.41|1.4 MB|7.8 kB/s|
|axum|87.68µs|5.98ms|11,959|66.15|1.6 MB|8.9 kB/s|
|go|261.69µs|4.38ms|11,961|66.16|1.6 MB|8.9 kB/s|
|hono|426.24µs|6.46ms|11,965|66.21|2.2 MB|12 kB/s|
|fastify|489.87µs|7.62ms|11,964|66.22|2.4 MB|13 kB/s|
|express|1.04ms|7.2ms|11,964|66.18|3.1 MB|17 kB/s

The performance of Ecewo is:

- ~1.4x faster than Axum, but they are almost same
- ~4.2x faster than Go net/http
- ~6.8x faster than Hono
- ~7.8x faster than Fastify
- ~16.6x faster than Express

### Documentation

Refer to the [docs](https://ecewo.vercel.app) for usage.

### License

Licensed under [MIT](./LICENSE).
