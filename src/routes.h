#ifndef ROUTES_H
#define ROUTES_H

#include <stddef.h>

#include "hashmap.h"
#include "request.h"
#include "response.h"

typedef struct http_response *(*route_handler)(
    const struct http_request *request, Hashmap *params);

enum route_type { ROUTE_EXACT, ROUTE_PARAM };

struct route {
  enum route_type type;
  const char *path;
  route_handler handler;
};

#define route(ROUTE)                                                           \
  struct http_response *route_##ROUTE(const struct http_request *request,      \
                                      Hashmap *params)

#define route_name(ROUTE) route_##ROUTE

route(root);
route(echo);
route(user_agent);
route(files);
static struct route routes[] = {
    {ROUTE_EXACT, "/", route_name(root)},
    {ROUTE_PARAM, "/echo/", route_name(echo)},
    {ROUTE_EXACT, "/user-agent", route_name(user_agent)},
    {ROUTE_PARAM, "/files/", route_name(files)},
};
extern const size_t routes_len;

#endif
