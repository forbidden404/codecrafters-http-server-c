#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "bstrlib.h"
#include "hashmap.h"
#include "response.h"
#include "routes.h"

#define MESSAGE_SIZE 64

const size_t routes_len = 3;

struct http_response *route_root(const struct http_request *request,
                                 const char *param) {
  if (strncmp(request->method, "GET", 3) != 0) {
    return NULL;
  }

  return create_empty_http_1_1_message(HTTP_OK, "OK");
}

struct http_response *route_echo(const struct http_request *request,
                                 const char *param) {
  return create_plain_message(param);
}

struct http_response *route_user_agent(const struct http_request *request,
                                       const char *param) {
  bstring result = Hashmap_get(request->headers, bfromcstr("User-Agent"));
  if (result == NULL) {
    return NULL;
  }

  return create_plain_message(bstr2cstr(result, 0));
}
