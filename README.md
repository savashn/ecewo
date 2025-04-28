![ecewo](https://raw.githubusercontent.com/savashn/ecewo/main/ecewo/assets/ecewo.svg)

<br />

ecewo is a minimal HTTP framework for C. No worries; ecewo takes the hard part of C programming and lets you build your backend easily.

## Requirements

GCC is required to compile and run the program, that's all. Please note that ecewo is running on Windows only for now. Support for Linux and macOS is planned to be added in the future.

## Quick Start

### 0. Clone the repo:

```
git clone https://github.com/savashn/ecewo.git
cd ecewo
```

### 1. Set up your src:

```
mkdir src
cd src
ni main.c
ni handlers.c
ni handlers.h
```

### 2. Write your first handler:

```sh
// src/handlers.c

#include "ecewo/router.h"

void hello_world(Req *req, Res *res)
{
    reply(res, "200 OK", "text/plain", "hello world!");
}

```

And declare it in the `handlers.h`:

```sh
// src/handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo/router.h"

void hello_world(Req *req, Res *res);

#endif
```

### 3. Set up your entry point:

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

### 4. Update makefile

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

### 5. Run and send a request

Run `make build` command in your terminal and go to `http://localhost:3000/`.

## Documentation

Refer to the [docs](https://ecewo.vercel.app/docs) to start building a backend with ecewo.

## Table of Contents

1. [Installation](https://ecewo.vercel.app/docs/installation)
    - 1.1 [Requirements](https://ecewo.vercel.app/docs/installation#requirements)
    - 1.2 [Install](https://ecewo.vercel.app/docs/installation#install)
    - 1.3 [Update](https://ecewo.vercel.app/docs/installation#update)
    - 1.4 [Makefile](https://ecewo.vercel.app/docs/installation#makefile)
        - 1.4.1 [CFLAGS](https://ecewo.vercel.app/docs/installation#cflags)
        - 1.4.2 [LDFLAGS](https://ecewo.vercel.app/docs/installation#ldflags)
        - 1.4.3 [SRC](https://ecewo.vercel.app/docs/installation#src)
        - 1.4.4 [Shortcuts](https://ecewo.vercel.app/docs/installation#shortcuts)
    - 1.5 [Start Server](https://ecewo.vercel.app/docs/start-server)
2. [Route Handling](https://ecewo.vercel.app/docs/route-handling)
    - 2.1 [Handlers](https://ecewo.vercel.app/docs/route-handling#handlers)
    - 2.2 [Declaring Routes](https://ecewo.vercel.app/docs/route-handling#declaring-routes)
    - 2.3 [Notes](https://ecewo.vercel.app/docs/route-handling#notes)
3. [Handling Requests](https://ecewo.vercel.app/docs/handling-requests)
    - 3.1 [Request Body](https://ecewo.vercel.app/docs/handling-requests#request-body)
    - 3.2 [Request Params](https://ecewo.vercel.app/docs/handling-requests#request-params)
    - 3.3 [Request Query](https://ecewo.vercel.app/docs/handling-requests#request-query)
    - 3.4 [Request Headers](https://ecewo.vercel.app/docs/handling-requests#request-headers)
4. [Using cJSON](https://ecewo.vercel.app/docs/using-cjson)
    - 4.1 [Creating JSON](https://ecewo.vercel.app/docs/using-json#creating-json)
    - 4.2 [Parsing JSON](https://ecewo.vercel.app/docs/using-json#parsing-json)
5. [Using A Database](https://ecewo.vercel.app/docs/using-a-database)
    - 5.1 [Install SQLite](https://ecewo.vercel.app/docs/using-a-database#install-sqlite)
    - 5.2 [Example Folder Structure](https://ecewo.vercel.app/docs/using-a-database#example-folder-structure)
    - 5.3 [Change The Makefile](https://ecewo.vercel.app/docs/using-a-database#change-the-makefile)
    - 5.4 [Connecting To Database](https://ecewo.vercel.app/docs/using-a-database#connecting-to-database)
    - 5.5 [Example Usage](https://ecewo.vercel.app/docs/using-a-database#example-usage)
        - 5.5.1 [Inserting Data](https://ecewo.vercel.app/docs/using-a-database#inserting-data)
        - 5.5.2 [Querying Data](https://ecewo.vercel.app/docs/using-a-database#querying-data)
6. [Authentication](https://ecewo.vercel.app/docs/authentication)
    - 6.1 [Login](https://ecewo.vercel.app/docs/authentication#login)
    - 6.2 [Logout](https://ecewo.vercel.app/docs/authentication#logout)
    - 6.3 [Getting session data](https://ecewo.vercel.app/docs/authentication#getting-session-data)
    - 6.4 [Protected Routes](https://ecewo.vercel.app/docs/authentication#protected-routes)
    - 6.5 [Notes](https://ecewo.vercel.app/docs/authentication#notes)
