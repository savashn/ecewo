# Cookie

Ecewo offers `get_cookie()` and `set_cookie()` functions to get or set a cookie header.

## Table of Contents

1. [Getting Cookies](#getting-cookies)
2. [Setting Cookies](#setting-cookies)

## Getting Cookies

```c
void cookie_reader(Req *req, Res *res) {
    char *session_id = get_cookie(req, "session_id");
    char *user_pref = get_cookie(req, "user_preference");
    
    if (session_id) {
        printf("Session ID: %s\n", session_id);
        printf("User preferences: %s\n", user_pref);
        send_text(res, OK, "Welcome back!");
        return;
    } else {
        send_text(res, UNAUTHORIZED, "No session");
        return;
    }
}
```

## Setting Cookies

The following `cookie_t` structure is required for `set_cookie()`.

```c
typedef struct
{
    int max_age;        // Default: -1
    char *path;         // Default: "/"
    char *domain;       // Optional
    char *same_site;    // Default: NULL
    bool http_only;     // Default: false
    bool secure;        // Default: false
} cookie_t;
```

```c
void login_handler(Req *req, Res *res) {
    // Set simple cookie
    set_cookie(res, "theme", "dark", NULL);


    // Set complex cookie
    cookie_t opts = {
        .max_age = 3600,    // 1 hour
        .path = "/",
        .same_site = "None",
        .http_only = true,  // Prevent JS access
        .secure = true,     // HTTPS only
    }
    
    set_cookie(res, "session_id", "session_id_here", &opts);
    send_text(res, OK, "Logged in");
}

void logout_handler(Req *req, Res *res) {
    // Delete cookie by setting max_age to 0
    cookie_t opts = {
        opts.max_age = 0,
    };
    
    set_cookie(res, "session_id", "", &opts);
    send_text(res, 200, "Logged out");
}
```
