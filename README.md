# Documentation

- 1. [Introduction](#introduction)
- 2. [Installation](#installation)
  	- 2.1 [Requirements](#requirements)
  	- 2.2 [Install](#install)
  	- 2.3 [Update](#update)
  	- 2.4 [Makefile](#makefile)
- 3. [Folder Structure](#folder-structure)
- 4. [Start Server](#start-server)
- 5. [Route Handling](#route-handling)
  	- 5.1 [Handlers](#handlers)
  	- 5.2 [Routes](#routes)
  	- 5.3 [Using A Database](#using-a-database)
- 6. [Handling Requests](#handling-requests)
  	- 6.1 [Request Body](#request-body)
  	- 6.2 [Request Params](#request-params)
  	- 6.3 [Request Query](#request-query)
  	- 6.4 Request Headers (oncoming feature)
 
## Introduction
ecewo is a HTTP server written in C. It's not production ready yet and it doesn't need to be, because I'm building it as a hobby project to better understand programming and memory allocation.

## Installation
### Requirements

ecewo is running on Windows only for now. You need `MINGW-64` to compile and run the program.

### Install

Since ecewo doesn't use any package manager, you need to clone this repo to use it. Follow these steps to clone:

```sh
git clone https://github.com/savashn/ecewo.git
cd ecewo
```

### Update

To update ecewo, simply copy the entire `ecewo` folder and replace your existing one.
Make sure that the system files starting with `ecewo/` listed in the SRC section of your `makefile` haven’t changed.
If they have, copy the SRC list from the new version’s `makefile` and paste it into your existing one.

### Makefile

The `makefile` is one of the most important file of your program. You must make the configuration of your `makefile` correct, otherwise you will get errors while trying to compile your program.
There is an important config named `SRC` for your server in it and you need to deal with it while you're developing your server.
When you creating a new `.c` file, you must add the filename into `SRC`.
At the begining, there will be some root files in it:

```sh
SRC = \
	ecewo/server.c \
	ecewo/router.c \
	ecewo/lib/sqlite3.c \
	ecewo/lib/cjson.c \
	ecewo/lib/params.c \
	ecewo/lib/query.c \
	src/main.c \
	src/handlers.c \
```

For example; if you create a new `database.c` file in the `src` directory, you must add `src/database.c \` in the `SRC` list to compile that file.

There are some shortcuts at the bottom of the `makefile`:

```sh
make		// compile the program
make run	// run the program
make build	// compile the program, create a db and run
make clean	// destroy the existing 'program.exe'
make clean-db	// destroy the existing 'db.sql'
make nuke	// destroy both the 'program.exe' and 'db.sql'
make rebuild	// destroy all, recompile and run the program
make compile	// destroy the program, recompile and run
make migrate	// destroy the database, recreate and run
```

## Folder Structure

When you cloned this repo, you'll see a folder structure like this:

<pre>
your-project/
├── ecewo/
├── src/
│   └── main.c
│   └── handlers.c
│   └── handlers.h
│   └── routes.h
└── makefile
</pre>

There are system files in the `ecewo` folder, so you don't need to touch it. You'll use the `src` only. Let's explain the inside of the `src` folder:

- `main.c` is the main file of your program. The whole server is starting by it.
- `handlers.c` is the file that you write your controllers/handlers in it.
- `handlers.h` is the file that you define your handlers in it to reach them in the `routes.h` file.
- `routes.h` is the file that includes your endpoints.

Generaly, you will rarely play with `main.c`.

## Start Server

When you open the `main.c` file, you'll see some basic configurations in it:

```sh
#include <stdio.h>
#include "ecewo/server.h"
#include "db.h"

int main()
{
  init_db();		// Create a database
  ecewo();		// Run the server
  sqlite3_close(db);	// Close the database connection
  return 0;		// Exit function
}
```

The `ecewo/server.h` is the main tool to run the HTTP server.
Run `make build` command to compile and run the program.
We'll see following informations when our server is ready:

```sh
Database connection successful
ecewo v0.8.0
Server is running on: http://localhost:4000
```

When the program compiled and ran for the first time, two additional files will be created in the root directory: `program.exe` and `sql.db`.
Since ecewo has built-in SQLite database, a database file is creating automaticly. We will deal with SQLite later.

The server will be running at `http://localhost:4000/`. You'll see this basic JSON message if you go to `localhost:4000`:

```sh
{"hello":"world"}
```

If you see this message, everything is all right. Your server is working.

## Route Handling
### Handlers

ecewo has strong built-in JSON library named [cJSON](https://github.com/DaveGamble/cJSON), thanks to [Dave Gamble](https://github.com/DaveGamble).
So we are able to play with JSON objects easily.

You'll see `handlers.c` and `handlers.h` files in the `src` folder in the root. They are default example files.
You can play with them yourself later, but now we need to see what does they do.
Firstly, we'll deal with the `handlers.c` file. Check the following `hello world` example:

```sh
// src/handlers.c

#include <stdio.h>
#include "ecewo/router.h"
#include "ecewo/lib/cjson.h"

void hello_world(Req *req, Res *res)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "hello", "world");

    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
}
```

First, let's explain what is going on in the `handlers.c` file. We include at starting:
- `<stdio.h>` to manage the memory,
- `"ecewo/router.h` to handle request and send response,
- `"ecewo/lib/cjson.h"` to deal with JSON objects.

We have two main parameters for our handlers: `Req *req` for requests and `Res *res` for responses. We will see them more detailed in the next example.

When we are done with the handler, we should send a response to the client. We use `reply()` function to send the response we prepared.
The `reply()` function takes 4 parameters:
- The `res` object,
- Status code,
- Content-type,
- Response body

We must free the memory we allocated when we are done with our handler. In this example, the `cJSON_Delete(json)` and `free(json_string)` free the JSON memory.

When we finished writing the `hello world` handler, we should define it to route the request.
Here is the `handlers.h` joining in the game:

```sh
// src/handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo/router.h"

void hello_world(Req *req, Res *res);

#endif
```

All right, now we can route the request to the handler in the `routes.h` file.

### Routes

```sh
#ifndef ROUTES_H
#define ROUTES_H

#include "ecewo/router.h"
#include "handlers.h"

Router routes[] = {
    {"GET", "/", hello_world},
};

#endif
```

We should add all of our handlers to the `routes[]` array to route the requests to the handlers. We just wrote a `hello world` handler, so we add it to the array as `{"GET", "/", hello_world},`.
The first parameter `GET` is the request method, the second one is the route, and the third one is our handler.

So finally, we can run these commands:

```sh
make
make run
```

After that, if you go to the `http://localhosh:4000/` address, you'll see this:
```sh
{
	"hello": "world"
}
```

### Using A Database

ecewo has built-in SQLite database. If you look at the `src/db.c`, you'll see `init_db` function that creates a database:

```sh
// src/db.c

#include <stdio.h>
#include "ecewo/lib/sqlite3.h"

sqlite3 *db = NULL;

int init_db()
{
    int rc = sqlite3_open("sql.db", &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open the database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("Database connection successful\n");
    return 0;
}
```

The `init_db()` function is our main function to configure our database.
But this function is just creates a database, not tables.
Now let's create a table. Add the following script to `db.c` file, before the `init_db()` function:

```sh
int create_table()
{
    const char *create_table =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "surname TEXT NOT NULL,"
        "username TEXT NOT NULL"
        ");";

    char *err_msg = NULL;

    int rc = sqlite3_exec(db, create_table, 0, 0, &err_msg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot create the table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    printf("Database and tables are ready\n");
    return 0;
}
```

We wrote a function that creates a table, but we didn't use it yet. To run it, we should call `create_table()` function in the `init_db()`. So, our `db.c` file should look like this:

```sh
#include <stdio.h>
#include "ecewo/lib/sqlite3.h"

sqlite3 *db = NULL;

int create_table()
{
    const char *create_table =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "surname TEXT NOT NULL,"
        "username TEXT NOT NULL"
        ");";

    char *err_msg = NULL;

    int rc = sqlite3_exec(db, create_table, 0, 0, &err_msg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot create the table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    printf("Database and table are ready\n");
    return 0;
}

int init_db()
{
    int rc = sqlite3_open("sql.db", &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open the database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    create_table(); // WE ADDED THE FUNCTION THAT CREATES THE 'USERS' TABLE

    printf("Database connection successful\n");
    return 0;
}

```

Now we can rebuild our program by running `make rebuild` command in the terminal.

If everything's went OK, you can see the 'users' table if you open the `db.sql`.

## Handling Requests
### Request Body

We already created a 'Users' table. Now we will add a user to it. Let's begin with writing our POST handler:

```sh
// handlers.c

#include <stdio.h>
#include "ecewo/router.h"
#include "ecewo/lib/cjson.h"
#include "ecewo/lib/sqlite3.h"

extern sqlite3 *db; // THIS IS IMPORTANT TO USE THE DATABASE

void add_user(Req *req, Res *res)
{
    const char *body = req->body; // Reach the body of the request

    if (body == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing request body");
        return;
    }

    cJSON *json = cJSON_Parse(body); // Parse the body

    if (!json)
    {
        reply(res, "400 Bad Request", "text/plain", "Invalid JSON");
        return;
    }

    const char *name = cJSON_GetObjectItem(json, "name")->valuestring;		// Take the 'name' field
    const char *surname = cJSON_GetObjectItem(json, "surname")->valuestring;	// Take the 'surname' field
    const char *username = cJSON_GetObjectItem(json, "username")->valuestring;	// Take the 'username' field

    if (!name || !surname || !username)
    {
        cJSON_Delete(json);
        reply(res, "400 Bad Request", "text/plain", "Missing fields");
        return;
    }

    const char *sql = "INSERT INTO users (name, surname, username) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        cJSON_Delete(json);
        reply(res, "500 Internal Server Error", "text/plain", "DB prepare failed");
        return;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, surname, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    cJSON_Delete(json);

    if (rc != SQLITE_DONE)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB insert failed");
        return;
    }

    reply(res, "201 Created", "text/plain", "User created!");
}
```

Add to `handlers.h` too:

```sh
#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo/router.h"

void hello_world(Req *req, Res *res);
void add_user(Req *req, Res *res); // DEFINE HERE THE 'add_user' HANDLER

#endif
```

Also into the Routes array in `routes.h`:

```sh
#ifndef ROUTES_H
#define ROUTES_H

#include "ecewo/router.h"
#include "handlers.h"

Router routes[] = {
	{"POST", "/user", add_user}, // SPECIFIC ROUTES SHOULD BE ON THE TOP
	{"GET", "/", hello_world},
};

#endif
```

Let's rebuild our server by running `make rebuild` command in the terminal. And then we'll send a `POST` request at `http://localhost:4000/user`.
You can use `POSTMAN` or something else to send requests.

We'll send a request, which has a body like:
```sh
{
    "name": "John",
    "surname": "Doe",
    "username": "johndoe",
}
```

If everything is correct, the output will be `User created!`.

Send one more request too for next example:

```sh
{
    "name": "Jane",
    "surname": "Doe",
    "username": "janedoe",
}
```

Now we'll write a handler function that gives us these two users' information.
But let's say that "name" and "surname" fields are not required for us, so we need "id" and "username" fields only.
To do this, in `headers.c`:

```sh
void get_all_users(Req *req, Res *res)
{
    const char *sql = "SELECT * FROM users;";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB prepare failed");
        return;
    }

    cJSON *json_array = cJSON_CreateArray();

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const int id = sqlite3_column_int(stmt, 0);                        // 0 is the index of the 'id' column
        const char *username = (const char *)sqlite3_column_text(stmt, 3); // 3 is the index of the 'username' column

        cJSON *user_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(user_json, "id", id);
        cJSON_AddStringToObject(user_json, "username", username);

        cJSON_AddItemToArray(json_array, user_json);
    }

    if (rc != SQLITE_DONE)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB step failed");
        sqlite3_finalize(stmt);
        return;
    }

    char *json_string = cJSON_PrintUnformatted(json_array);

    reply(res, "200 OK", "application/json", json_string); // Send the response

    // Free the allocated memory when we are done:

    cJSON_Delete(json_array);
    free(json_string);
    sqlite3_finalize(stmt);
}
```

Why didn't we use `for` instead of `while`?

We could use `for` loop too, but it would look more complicated and would have less readability.

If we wrote it with `for` it would be like this:
```sh
for (rc = sqlite3_step(stmt); rc == SQLITE_ROW; rc = sqlite3_step(stmt))
{
    // ...
}
```

... instead of this:
```sh
while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
{
    // ...
}
```

You can use `for` loop if you want, but `while` loop is more readable for this job as you can see.

Now let's define this handler in `handlers.h` and add to the `routes[]` in the `routes.h`:

```sh
// handlers.h:

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo/router.h"

void hello_world(Req *req, Res *res);
void get_all_users(Req *req, Res *res);
void add_user(Req *req, Res *res);

#endif
```

```sh
// routes.h:

#ifndef ROUTES_H
#define ROUTES_H

#include "ecewo/router.h"
#include "handlers.h"

Router routes[] = {
    {"GET", "/users", get_all_users},
    {"POST", "/user", add_user},
    {"GET", "/", hello_world},
};

#endif
```

Now send a requests to the `http://localhost:4000/users`, and you'll receive this output:

```sh
[
    {
        "id": 1,
        "username": "johndoe"
    },
    {
        "id": 2,
        "username": "janedoe"
    }
]
```

### Request Params

Let's take a specific user by params. We can access the params by `get_params()` function. Let's write a handler that gives us the "Jane Doe" by username.
But first, add `routes.h` the route:

```sh
// routes.h:

Router routes[] = {
    {"GET", "/users/:slug", get_user_by_params},
};
```

Now we need to write the handler `get_user_by_params`:

```sh
// handlers.c:

void get_user_by_params(Req *req, Res *res)
{
    const char *slug = params_get(&req->params, "slug");

    if (slug == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing 'id' parameter");
        return;
    }

    const char *sql = "SELECT name, surname FROM users WHERE username = ?;"; // Our SQL query

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB prepare failed");
        return;
    }

    sqlite3_bind_text(stmt, 1, slug, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW)
    {
        const char *name = (const char *)sqlite3_column_text(stmt, 0);		// 0 is the index of "name"
        const char *surname = (const char *)sqlite3_column_text(stmt, 1);	// 1 is the index of "surname"

        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "name", name);
        cJSON_AddStringToObject(json, "surname", surname);

        char *json_string = cJSON_PrintUnformatted(json);

        reply(res, "200 OK", "application/json", json_string);

        cJSON_Delete(json); // free cJSON memory
        free(json_string);  // free json_string memory
    }
    else
    {
        reply(res, "404 Not Found", "text/plain", "User not found");
    }

    sqlite3_finalize(stmt); // free sql memory
}
```

```sh
// handlers.h:

void get_user_by_params(Req *req, Res *res)
```

Run the `make compile` command and send a request to `http://localhost:4000/users/janedoe`. We'll receive this:

```sh
{
  "name": "Jane",
  "surname": "Doe"
}
```

### Request Query

Like the `get_params()`, we also have `get_query()` function to get the query in the request. Let's rewrite the same handler using `get_query()` this time: 

```sh
// routes.h:

Router routes[] = {
    {"GET", "/users", get_user_by_query},
};
```

```sh
// handlers.c:

void get_user_by_query(Req *req, Res *res)
{
    const char *username = query_get(&req->query, "username");

    if (username == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing required parameter: username");
        return;
    }

    const char *sql = "SELECT name, surname FROM users WHERE username = ?;"; // Our SQL query

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB prepare failed");
        return;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW)
    {
        const char *name = (const char *)sqlite3_column_text(stmt, 0);
        const char *surname = (const char *)sqlite3_column_text(stmt, 1);

        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "name", name);
        cJSON_AddStringToObject(json, "surname", surname);

        char *json_string = cJSON_PrintUnformatted(json);

        reply(res, "200 OK", "application/json", json_string);

        cJSON_Delete(json); // free cJSON memory
        free(json_string);  // free json_string memory
    }
    else
    {
        reply(res, "404 Not Found", "text/plain", "User not found");
    }

    sqlite3_finalize(stmt); // free sql memory
}
```

```sh
// handlers.h:

void get_user_by_query(Req *req, Res *res)
```

Let's recompile the program by running `make compile` and send a request to `http//localhost:4000/users?username=johndoe`. We'll receive the output:

```sh
{
  "name": "John",
  "surname": "Doe"
}
```
