![ecewo](https://raw.githubusercontent.com/savashn/ecewo/main/assets/logo.svg)

# Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
  	- 2.1 [Requirements](#requirements)
  	- 2.2 [Install](#install)
  	- 2.3 [Update](#update)
  	- 2.4 [Makefile](#makefile)
3. [Folder Structure](#folder-structure)
4. [Start Server](#start-server)
5. [Route Handling](#route-handling)
  	- 5.1 [Handlers](#handlers)
  	- 5.2 [Routes](#routes)
  	- 5.3 [Using A Database](#using-a-database)
6. [Handling Requests](#handling-requests)
  	- 6.1 [Request Body](#request-body)
  	- 6.2 [Request Params](#request-params)
  	- 6.3 [Request Query](#request-query)
  	- 6.4 [Request Headers](#request-headers)
7. [Authentication](#authentication)
    - 7.1 [Login](#login)
    - 7.2 [Logout](#logout)
    - 7.3 [Getting session data](#getting-session-data)
    - 7.4 [Protected Routes](#protected-routes)
 
## Introduction
ecewo is a minimal HTTP framework in C. Uses cJSON and SQLite as embedded libraries – no external linking or installation required.
It's not production ready yet and it doesn't need to be, because I'm building it as a hobby project to better understand programming and memory allocation.

## Installation
### Requirements

ecewo is running on Windows only for now. You need `MINGW-64` to compile and run the program. Support for Linux and macOS is planned to be added in the future.

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
	ecewo/request.c \
	ecewo/lib/cjson.c \
	ecewo/lib/sqlite3.c \
	ecewo/lib/session.c \
	src/main.c \
	src/handlers.c \
	src/db.c \
```

For example; if you create a new `database.c` file in the `src` directory, you must add `src/database.c \` in the `SRC` list to compile that file.

There are some shortcuts at the bottom of the `makefile`:

```sh
make		// compile the program.
make run	// run the program.
make clean	// destroy the program.
make clean-db	// destroy the database.
make build	// rebuild the program, and then run it.
make build-all	// rebuild the program and the database, then run it.

```

## Folder Structure

When you cloned this repo, you'll see a folder structure like this:

<pre>
your-project/
├── ecewo/
│   ├── request.c
│   ├── request.h
│   ├── router.c
│   ├── router.h
│   ├── server.c
│   └── server.h
│   └── lib/
│       ├── cjson.c
│       ├── cjson.h
│       ├── session.c
│       ├── session.h
│       ├── sqlite3.c
│       ├── sqlite3.h
├── src/
│   ├── main.c
│   ├── handlers.c
│   ├── handlers.h
│   └── routes.h
└── makefile
</pre>

There are root files in the `ecewo` folder, so you don't need to touch it. You'll use the `src` only. Let's explain the inside of the `src` folder:

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
ecewo v0.10.0
Server is running at: http://localhost:4000
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

ecewo has a strong built-in JSON library named [cJSON](https://github.com/DaveGamble/cJSON), thanks to [Dave Gamble](https://github.com/DaveGamble).
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

All right, now we can route the request to the handler in the `routes.h` file. See the next chapter.

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

Finally, we can run `make build` in the terminal to rebuild our server.
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

Now we can rebuild our program by running `make build-all` command in the terminal.

If everything's went OK, you can see the 'users' table if you open the `db.sql`.

Note: execute `make build-all` if you want the database to be rebuilt from scratch during the build phase.
Execute `make build` if you only want to recompile the program without touching the database.

## Handling Requests
### Request Body

We already created a 'Users' table in the previously chapter. Now we will add a user to it. Let's begin with writing our POST handler:

```sh
// src/handlers.c

#include <stdio.h>
#include "ecewo/router.h"
#include "ecewo/lib/cjson.h"
#include "ecewo/lib/sqlite3.h"

extern sqlite3 *db; // THIS IS IMPORTANT TO USE THE DATABASE

// Function to add a user to the database
void add_user(Req *req, Res *res)
{
    const char *body = req->body; // Get the body of the request

    // If there is no body, return a 400 Bad Request response
    if (body == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing request body");
        return;
    }

    // Parse the body as JSON
    cJSON *json = cJSON_Parse(body);

    // If JSON parsing fails, return a 400 Bad Request response
    if (!json)
    {
        reply(res, "400 Bad Request", "text/plain", "Invalid JSON");
        return;
    }

    // Extract the 'name', 'surname', and 'username' fields from the JSON object
    const char *name = cJSON_GetObjectItem(json, "name")->valuestring;
    const char *surname = cJSON_GetObjectItem(json, "surname")->valuestring;
    const char *username = cJSON_GetObjectItem(json, "username")->valuestring;

    // If any of the required fields are missing, delete the JSON object and return a 400 error
    if (!name || !surname || !username)
    {
        cJSON_Delete(json);
        reply(res, "400 Bad Request", "text/plain", "Missing fields");
        return;
    }

    // SQL query to insert a new user into the database
    const char *sql = "INSERT INTO users (name, surname, username) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    // If the SQL preparation fails, return a 500 Internal Server Error
    if (rc != SQLITE_OK)
    {
        cJSON_Delete(json);
        reply(res, "500 Internal Server Error", "text/plain", "DB prepare failed");
        return;
    }

    // Bind the values to the SQL query
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, surname, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_STATIC);

    // Execute the SQL statement to insert the user
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    cJSON_Delete(json);

    // If the insert operation fails, return a 500 error
    if (rc != SQLITE_DONE)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB insert failed");
        return;
    }

    // If everything is successful, return a 201 Created response
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

Let's rebuild our server by running `make build` command in the terminal. And then we'll send a `POST` request at `http://localhost:4000/user`.
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

Send one more request for the next example:

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

We could have used a `for` loop too, but it would be more complicated and less readable.

If we had written it with a `for` loop, it would look like this:
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

Let's take a specific user by params. We can access the params using the `get_req(&req->params, "params")` function and free the memory with `free_req(&req->params)`. Let's write a handler that gives us the "Jane Doe" by username.
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
    const char *slug = get_req(&req->params, "slug"); // We got the params

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
	const char *errmsg = sqlite3_errmsg(db);
        fprintf(stderr, "SQLite error: %s\n", errmsg); // Log
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

        cJSON_Delete(json);		// free cJSON memory
        free(json_string);		// free json_string memory
    }
    else
    {
        reply(res, "404 Not Found", "text/plain", "User not found");
    }

    free_req(&req->params);	// free params memory
    sqlite3_finalize(stmt);	// free sql memory
}
```

```sh
// handlers.h:

void get_user_by_params(Req *req, Res *res)
```

Run the `make build` command and send a request to `http://localhost:4000/users/janedoe`. We'll receive this:

```sh
{
  "name": "Jane",
  "surname": "Doe"
}
```

### Request Query

Like the `params`, we can use `get_req(&req->query, "query")` to get the query and `free_req(&req->query)` to free the memory. Let's rewrite the same handler via `query` this time: 

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
    const char *username = get_req(&req->query, "username"); // We got the query

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

    free_req(&req->query);	// free query memory
    sqlite3_finalize(stmt);	// free sql memory
}
```

```sh
// handlers.h:

void get_user_by_query(Req *req, Res *res)
```

Let's recompile the program by running `make compile` and send a request to `http//localhost:4000/users?username=johndoe`. We'll receive that output:

```sh
{
  "name": "John",
  "surname": "Doe"
}
```

### Request Headers

As like as the `params` and `query`, we can reach also the headers of the request via `get_req(&req->headers, "header")` function.
We have some functions for authorization and authentication via sessions though, however, if you want to reach any item in the `request->headers`, you are able to do it.

Normally, a standard `GET` request with POSTMAN have some headers like:

```sh
{
    "User-Agent": "PostmanRuntime/7.43.3",
    "Accept": "*/*",
    "Postman-Token": "9b1c7dda-27f9-471a-9cdd-bfaf0d5b56a1",
    "Host": "localhost:4000",
    "Accept-Encoding": "gzip, deflate, br",
    "Connection": "keep-alive"
}
```

Let's say, we need the `User-Agent` header:

```sh
// handlers.c:

void handler_get_user_agent_header(Req *req, Res *res)
{
    const char *header = get_req(&req->headers, "User-Agent");

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "User Agent", header);

    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
    free_req(&req->headers);
}


// handlers.h:

void handler_get_user_agent_header(Req *req, Res *res);


// routes.h:

Router routes[] = {
    {"GET", "/header", handler_get_user_agent_header},
};
```

The output will be this:

```sh
{
    "User Agent": "PostmanRuntime/7.43.3"
}
```
