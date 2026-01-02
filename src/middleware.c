#include <stdlib.h>
#include <stdbool.h>
#include "middleware.h"
#include "route-trie.h"
#include "server.h"
#include "logger.h"

typedef struct
{
  MiddlewareHandler *handlers;
  RequestHandler route_handler;
  uint16_t count;
  uint16_t current;
} Chain;

MiddlewareHandler *global_middleware = NULL;
uint16_t global_middleware_count = 0;
uint16_t global_middleware_capacity = 0;

static void execute_next(Req *req, Res *res) {
  if (!req || !res) {
    LOG_ERROR("NULL request or response");
    return;
  }

  Chain *chain = (Chain *)req->chain;

  if (!chain) {
    LOG_ERROR("NULL chain");
    return;
  }

  if (chain->current < chain->count) {
    MiddlewareHandler mw = chain->handlers[chain->current++];
    mw(req, res, execute_next);
  } else {
    if (chain->route_handler)
      chain->route_handler(req, res);
  }
}

void chain_start(Req *req, Res *res, MiddlewareInfo *middleware_info) {
  if (!req || !res || !middleware_info || !middleware_info->handler)
    return;

  int total_middleware_count = global_middleware_count + middleware_info->middleware_count;

  if (total_middleware_count == 0) {
    middleware_info->handler(req, res);
    return;
  }

  MiddlewareHandler *combined_handlers = arena_alloc(req->arena, sizeof(MiddlewareHandler) * total_middleware_count);

  if (!combined_handlers) {
    LOG_ERROR("Arena allocation failed for middleware handlers.");
    middleware_info->handler(req, res);
    return;
  }

  arena_memcpy(combined_handlers,
               global_middleware,
               sizeof(MiddlewareHandler) * global_middleware_count);

  if (middleware_info->middleware_count > 0 && middleware_info->middleware) {
    arena_memcpy(combined_handlers + global_middleware_count,
                 middleware_info->middleware,
                 sizeof(MiddlewareHandler) * middleware_info->middleware_count);
  }

  Chain *chain = arena_alloc(req->arena, sizeof(Chain));
  if (!chain) {
    LOG_ERROR("Arena allocation failed for middleware chain.");
    middleware_info->handler(req, res);
    return;
  }

  chain->handlers = combined_handlers;
  chain->count = total_middleware_count;
  chain->current = 0;
  chain->route_handler = middleware_info->handler;

  req->chain = chain;

  execute_next(req, res);
}

void use(MiddlewareHandler middleware_handler) {
  if (!middleware_handler) {
    LOG_ERROR("NULL middleware handler");
    abort();
  }

  if (global_middleware_count >= global_middleware_capacity) {
    int new_cap = global_middleware_capacity ? global_middleware_capacity * 2 : INITIAL_MW_CAPACITY;
    MiddlewareHandler *tmp = realloc(global_middleware, new_cap * sizeof *tmp);
    if (!tmp) {
      LOG_ERROR("Reallocation failed in global middleware");
      abort();
    }
    global_middleware = tmp;
    global_middleware_capacity = new_cap;
  }

  global_middleware[global_middleware_count++] = middleware_handler;
}

void reset_middleware(void) {
  if (global_middleware) {
    free(global_middleware);
    global_middleware = NULL;
  }
  global_middleware_count = 0;
  global_middleware_capacity = 0;
}

void free_middleware_info(MiddlewareInfo *info) {
  if (info) {
    if (info->middleware) {
      free(info->middleware);
      info->middleware = NULL;
    }
    free(info);
  }
}
