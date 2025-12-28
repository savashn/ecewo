<div align="center">
    <img src="https://raw.githubusercontent.com/savashn/ecewo/main/.github/assets/ecewo.svg" />
</div>

<div align="center">
    <h1>Express-C Effect for Web Operations</h1>
    A web framework for C — inspired by <a href="https://expressjs.com">express.js</a>
</div>

## Table of Contents

- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Benchmarks](#benchmarks)
- [Dependencies](#dependencies)
- [Running Tests](#running-tests)
- [Documentation](#documentation)
- [Modules](#modules)
- [Example App](#example-app)
- [Contributing](#contributing)
- [License](#license)

## Requirements

- A C compiler (GCC or Clang)
- CMake version 3.14 or higher

## Quick Start

**main.c:**
```c
#include "ecewo.h"
#include <stdio.h>

void hello_world(Req *req, Res *res)
{
    (void)req;
    send_text(res, OK, "Hello, World!");
}

int main(void)
{
    if (server_init() != 0)
    {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }

    get("/", hello_world);

    if (server_listen(3000) != 0)
    {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    server_run();
    return 0;
}
```

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.14)
project(myproject VERSION 1.0.0 LANGUAGES C)

include(FetchContent)

FetchContent_Declare(
    ecewo
    GIT_REPOSITORY https://github.com/savashn/ecewo.git
    GIT_TAG v3.0.0
)

FetchContent_MakeAvailable(ecewo)

add_executable(${PROJECT_NAME}
    main.c
)

target_link_libraries(${PROJECT_NAME} PRIVATE ecewo)
```

**Build:**

```shell
mkdir build && cd build && cmake .. && cmake --build .
```

## Benchmarks

Here are "Hello World" benchmark results comparing several frameworks with ecewo. See the source code of the [benchmark test](https://github.com/savashn/ecewo-benchmarks).

- **Machine:** 12th Gen Intel Core i7-12700F x 20, 32GB RAM, SSD
- **OS:** Fedora Workstation 43
- **Method:** `wrk -t8 -c100 -d40s http://localhost:3000` * 2, taking the second results.

| Framework | Req/Sec   | Transfer/Sec |
|-----------|-----------|--------------|
| ecewo     | 1,208,226 | 178.60 MB    |
| axum      | 1,192,785 | 168.35 MB    |
| go        | 893,248   | 115.85 MB    |
| express   | 93,214    | 23.20 MB     |

## Dependencies

ecewo is built on top of [libuv](https://github.com/libuv/libuv) and [llhttp](https://github.com/nodejs/llhttp). They are fetched automatically by CMake, so no manual installation is required.

## Running Tests

```shell
mkdir build
cd build
cmake -DECEWO_BUILD_TESTS=ON ..
cmake --build .
ctest
```

## Documentation

Refer to the [docs](/docs/) for usage.

## Modules

- [`ecewo-cluster`](https://github.com/savashn/ecewo-modules/tree/main/src/cluster) for multithreading.
- [`ecewo-cookie`](https://github.com/savashn/ecewo-modules/tree/main/src/cookie) for cookie management.
- [`ecewo-cors`](https://github.com/savashn/ecewo-modules/tree/main/src/cors) for CORS impelentation.
- [`ecewo-fs`](https://github.com/savashn/ecewo-modules/tree/main/src/fs) for file operations.
- [`ecewo-helmet`](https://github.com/savashn/ecewo-modules/tree/main/src/helmet) for automatically setting safety headers.
- [`ecewo-mock`](https://github.com/savashn/ecewo-modules/tree/main/src/mock) for mocking requests.
- [`ecewo-postgres`](https://github.com/savashn/ecewo-modules/tree/main/src/postgres) for async PostgreSQL integration.
- [`ecewo-session`](https://github.com/savashn/ecewo-modules/tree/main/src/session) for session management.
- [`ecewo-static`](https://github.com/savashn/ecewo-modules/tree/main/src/static) for static file serving.

## Example App

[Here](https://github.com/savashn/ecewo-example) is an example blog app built with ecewo and PostgreSQL.

## Contributing

Contributions are welcome. Please feel free to submit a pull requests or open issues for feature requests or bugs. See the [CONTRIBUTING.md](/CONTRIBUTING.md).

## License

Licensed under [MIT](./LICENSE).
