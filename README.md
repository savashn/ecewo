<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/ecewo/assets/ecewo.svg" />
    </a>
</div>

### Built for Modern Web Development in C

A modern and developer-friendly backend framework for C that handles the complexities of C programming and lets you build backends with ease — inspired by the simplicity of Express.js.

**Ecewo is a hobby project I started to improve my programming skills and it is still work in progress. So, it’s not stable or production-ready yet. See [FAQ](https://ecewo.vercel.app/docs/faq).**

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

And then run the following commands to start:

For Linux/MacOS:
```
chmod +x create.sh
./create.sh
```

For Windows:
```
./create.bat
```

These commands will automatically create a `hello world` example and generate a new script file. Run the following commands to build and start the server at `http://localhost:4000`.

For Linux/MacOS:
```
chmod +x ecewo.sh
./ecewo.sh --run
```

For Windows:
```
./ecewo.bat /run
```

### Example 'Hello World'

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
    text(200, "hello world!");
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
    final_router();
    return 0;
}
```

### Documentation

Refer to the [docs](https://ecewo.vercel.app) for usage.

### License

Licensed under [MIT](./LICENSE).
