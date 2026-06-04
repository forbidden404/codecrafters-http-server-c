#ifndef ROUTES_H
#define ROUTES_H

#include <stddef.h>

#include "request.h"
#include "response.h"

typedef struct http_response *(*route_handler)(
    const struct http_request *request, const char *param);

enum route_type { ROUTE_EXACT, ROUTE_PARAM };

struct route {
  enum route_type type;
  const char *path;
  route_handler handler;
};

struct http_response *route_root(const struct http_request *request,
                                 const char *param);
struct http_response *route_echo(const struct http_request *request,
                                 const char *param);
struct http_response *route_user_agent(const struct http_request *request,
                                       const char *param);

static struct route routes[] = {
    {ROUTE_EXACT, "/", route_root},
    {ROUTE_PARAM, "/echo/", route_echo},
    {ROUTE_EXACT, "/user-agent", route_user_agent},
};
extern const size_t routes_len;

#endif
