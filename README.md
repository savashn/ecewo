<div align="center">
    <a href="https://ecewo.vercel.app">
        <img src="https://raw.githubusercontent.com/savashn/ecewo/main/assets/ecewo.svg" />
    </a>
</div>

<hr />

Minimalist and unopinionated web framework designed to make modern web development in C easier — inspired by the simplicity of Express.js and the modularity of Fastify’s plugin architecture.

### In short, Ecewo is:
- **Modern**
- **Minimalist**
- **Lightweight**
- **Low overhead**
- **Developer friendly**
- **Unopinionated**
- **Fast**

Ecewo is still a work in progress, so it’s not stable or production-ready yet. See [FAQ](https://ecewo.vercel.app/docs/faq).

<hr />

### Table of Contents

- [Out of The Box Features](#out-of-the-box-features)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Example "Hello World"](#example-hello-world)
- [Documentation](#documentation)
- [Plugins](#plugins)
- [License](#license)

### Out of The Box Features

- Full asynchronous operations support
- Built-in JSON and CBOR support
- Cookie management and session or JWT based authentication mechanism
- Easy management of environment variables
- Flexible middleware support (route-specific and global)
- Express.js-like routing mechanism

### Requirements

- CMake version 3.10 or higher
- A C compiler (GCC, Clang, or MSVC)
- Git
- MSYS2 and Git Bash if you are using Windows

### Quick Start

Install Ecewo CLI:

```bash
curl -o installer.sh "https://raw.githubusercontent.com/savashn/ecewo/main/installer.sh" && chmod +x installer.sh && ./installer.sh
```

This command installs the Ecewo CLI to your user path. To install it to the system path for all users, run the following command with administrator privileges:

```bash
curl -o installer.sh "https://raw.githubusercontent.com/savashn/ecewo/main/installer.sh" && chmod +x installer.sh && ./installer.sh --admin
```

And then run the following commands to start:

```
ecewo create
```

This command will automatically create a `hello world` example and generate a new script file. Run the following command to build and start the server at `http://localhost:4000`.

```
ecewo run
```

Run `ecewo` in the terminal to see all the CLI commands.

### Example "Hello World"

```c
// src/handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo.h"

void hello_world(Req *req, Res *res);

#endif
```

```c
// src/handlers.c

#include "handlers.h"

void hello_world(Req *req, Res *res)
{
    text(200, "hello world!");
}

```

```c
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

### Plugins

In Ecewo, almost everything is a **plugin**. You can install them using Ecewo CLI. Here is a list of them:

- `cjson` for [JSON parsing and generation](https://ecewo.vercel.app/docs/using-json)
- `cbor` for [handling CBOR binaries](https://ecewo.vercel.app/docs/using-cbor)
- `async` for processing [async functions](https://ecewo.vercel.app/docs/async-operations)
- `dotenv` for easy [.env configuration](https://ecewo.vercel.app/docs/environment-variables)
- `sqlite` for [using SQLite database](https://ecewo.vercel.app/docs/using-a-database)
- `cookie` for [cookie management](https://ecewo.vercel.app/docs/auth#cookies)
- `session` for authentication with [sessions](https://ecewo.vercel.app/docs/auth#sessions)

### License

Licensed under [MIT](./LICENSE).
