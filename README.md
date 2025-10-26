<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/assets/ecewo.svg" />
    </a>
</div>

<hr />

## Express-C Effect for Web Operations

Modular C web framework with [Express.js](https://expressjs.com/) ergonomics and native performance.

> **This is a hobby project that I'm developing to improve my programming skills. So it might not be production-ready, and it doesn't have to be.**

## Table of Contents

- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Benchmarks](#benchmarks)
- [Documentation](#documentation)
- [Modules](#modules)
- [Example App](#example-app)
- [Contributing](#contributing)
- [License](#license)

## Requirements

- A C compiler
- CMake version 3.14 or higher

## Quick Start

**main.c:**
```c
#include "ecewo.h"
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

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.14)
project(myproject VERSION 1.0.0 LANGUAGES C)

include(FetchContent)

FetchContent_Declare(
    ecewo
    GIT_REPOSITORY https://github.com/savashn/ecewo.git
    GIT_TAG v2.2.0
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

Here are 'Hello World' benchmark results for several frameworks compared to Ecewo. See the source code of the [benchmark test](https://github.com/savashn/ecewo-benchmarks).

Lower is better.

| Framework  | Average | Median  | Max     | P90    | P95    |
| ---------- | ------- | ------- | ------- | ------ | ------ |
| Ecewo      | 0.387ms | 0.152ms | 7.23ms  | 0.99ms | 1.09ms |
| Axum       | 0.442ms | 0.505ms | 5.61ms  | 1.01ms | 1.21ms |
| Go         | 0.958ms | 0.725ms | 12.62ms | 1.97ms | 2.48ms |
| Express.js | 1.85ms  | 1.58ms  | 11.05ms | 3.48ms | 4.27ms |

## Documentation

Refer to the [docs](/docs/) for usage.

## Modules

| MODULE                                          | TODO                                                                                                  |
|-------------------------------------------------|-------------------------------------------------------------------------------------------------------|
| [`ecewo/postgres.h`](/include/ecewo/postgres.h) | [async PostgreSQL integration](/docs/08.async-database.md)                                            |
| [`ecewo/cookie.h`](/include/ecewo/cookie.h)     | [cookie management](/docs/12.cookie.md)                                                               |
| [`ecewo/session.h`](/include/ecewo/session.h)   | [session management](/docs/13.session.md)                                                             |
| [`ecewo/fs.h`](/include/ecewo/fs.h)             | [file operations](/docs/09.file-operations.md)                                                        |
| [`ecewo/static.h`](/include/ecewo/static.h)     | [static file serving](/docs/10.static-file-serving.md)                                                |
| [`ecewo/cors.h`](/include/ecewo/cors.h)         | [CORS implementation](/docs/14.cors.md)                                                               |
| [`ecewo/helmet.h`](/include/ecewo/helmet.h)     | [automaticaly setting safety headers](/docs/15.helmet.md)                                             |
| [`ecewo/mock.h`](/include/ecewo/mock.h)         | [mocking requests](/docs/16.testing.md) (comes with [Unity](https://github.com/ThrowTheSwitch/Unity)) |

## Example App

[Here](https://github.com/savashn/ecewo-example) is an example blog app built with Ecewo and PostgreSQL.

## Contributing

Contributions are welcome. Please feel free to submit a pull requests or open issues for feature requests or bugs.

## License

Licensed under [MIT](./LICENSE).
