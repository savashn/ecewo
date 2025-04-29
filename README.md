![ecewo](https://raw.githubusercontent.com/savashn/ecewo/main/ecewo/assets/ecewo.svg)

<br />

ecewo is a minimal HTTP framework for C that handles the complexities of C programming and helps you build your backend with ease.

### Table of Contents

- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Documentation](#documentation)
- [License](#license)

### Requirements

GCC is required to compile and run the program, that's all. Please note that ecewo is running on Windows only for now. Support for Linux and macOS is planned to be added in the future.

### Quick Start

Clone this repo:

```
git clone https://github.com/savashn/ecewo.git
cd ecewo
```

Set up a `src` folder:

```
mkdir src
cd src
ni main.c
ni handlers.c
ni handlers.h
```

Write a handler:

```sh
// src/handlers.c

#include "ecewo/router.h"

void hello_world(Req *req, Res *res)
{
    reply(res, "200 OK", "text/plain", "hello world!");
}

```

Declare the handler in `handlers.h`:

```sh
// src/handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo/router.h"

void hello_world(Req *req, Res *res);

#endif
```

Set up the enrtry point:

```sh
// src/main.c

#include "ecewo/server.h"
#include "ecewo/routes.h"
#include "handlers.h"

int main()
{
    get("/", hello_world);
    ecewo(3000);
    return 0;
}
```

Update `makefile`:

```
SRC = \
        ecewo/server.c \
        ecewo/router.c \
        ecewo/routes.c \
        ecewo/request.c \
        ecewo/lib/session.c \
        ecewo/lib/cjson.c \
        src/main.c \        # Add entry point
        src/handlers.c \    # Add handlers
```

Run `make build` command in your terminal and go to `http://localhost:3000/`.

### Documentation

Refer to the [docs](https://ecewo.vercel.app/docs) to start building a backend with ecewo.

1. [Installation](https://ecewo.vercel.app/docs/installation)
    - 1.1 [Requirements](https://ecewo.vercel.app/docs/installation#requirements)
    - 1.2 [Install](https://ecewo.vercel.app/docs/installation#install)
    - 1.3 [Update](https://ecewo.vercel.app/docs/installation#update)
    - 1.4 [Makefile](https://ecewo.vercel.app/docs/installation#makefile)
        - 1.4.1 [CFLAGS](https://ecewo.vercel.app/docs/installation#cflags)
        - 1.4.2 [LDFLAGS](https://ecewo.vercel.app/docs/installation#ldflags)
        - 1.4.3 [SRC](https://ecewo.vercel.app/docs/installation#src)
        - 1.4.4 [Shortcuts](https://ecewo.vercel.app/docs/installation#shortcuts)
2. [Star Server](https://ecewo.vercel.app/docs/start-server)
3. [Route Handling](https://ecewo.vercel.app/docs/route-handling)
    - 3.1 [Handlers](https://ecewo.vercel.app/docs/route-handling#handlers)
    - 3.2 [Declaring Routes](https://ecewo.vercel.app/docs/route-handling#declaring-routes)
    - 3.3 [Notes](https://ecewo.vercel.app/docs/route-handling#notes)
4. [Handling Requests](https://ecewo.vercel.app/docs/handling-requests)
    - 4.1 [Request Body](https://ecewo.vercel.app/docs/handling-requests#request-body)
    - 4.2 [Request Params](https://ecewo.vercel.app/docs/handling-requests#request-params)
    - 4.3 [Request Query](https://ecewo.vercel.app/docs/handling-requests#request-query)
    - 4.4 [Request Headers](https://ecewo.vercel.app/docs/handling-requests#request-headers)
5. [Using cJSON](https://ecewo.vercel.app/docs/using-cjson)
    - 5.1 [Creating JSON](https://ecewo.vercel.app/docs/using-json#creating-json)
    - 5.2 [Parsing JSON](https://ecewo.vercel.app/docs/using-json#parsing-json)
6. [Using A Database](https://ecewo.vercel.app/docs/using-a-database)
    - 6.1 [Install SQLite](https://ecewo.vercel.app/docs/using-a-database#install-sqlite)
    - 6.2 [Example Folder Structure](https://ecewo.vercel.app/docs/using-a-database#example-folder-structure)
    - 6.3 [Change The Makefile](https://ecewo.vercel.app/docs/using-a-database#change-the-makefile)
    - 6.4 [Connecting To Database](https://ecewo.vercel.app/docs/using-a-database#connecting-to-database)
    - 6.5 [Example Usage](https://ecewo.vercel.app/docs/using-a-database#example-usage)
        - 6.5.1 [Inserting Data](https://ecewo.vercel.app/docs/using-a-database#inserting-data)
        - 6.5.2 [Querying Data](https://ecewo.vercel.app/docs/using-a-database#querying-data)
7. [Authentication](https://ecewo.vercel.app/docs/authentication)
    - 7.1 [Login](https://ecewo.vercel.app/docs/authentication#login)
    - 7.2 [Logout](https://ecewo.vercel.app/docs/authentication#logout)
    - 7.3 [Getting session data](https://ecewo.vercel.app/docs/authentication#getting-session-data)
    - 7.4 [Protected Routes](https://ecewo.vercel.app/docs/authentication#protected-routes)
    - 7.5 [Notes](https://ecewo.vercel.app/docs/authentication#notes)

### License

Licensed under [MIT](./LICENSE).
