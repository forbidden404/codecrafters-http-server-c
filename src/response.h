#ifndef RESPONSE_H
#define RESPONSE_H

#include "hashmap.h"

enum http_status {
  HTTP_OK = 200,
  HTTP_CREATED = 201,
  HTTP_BAD_REQUEST = 400,
  HTTP_NOT_FOUND = 404,
  HTTP_INTERNAL_SERVER_ERROR = 500,
};

struct http_response;

struct http_response *create_plain_message(const char *message,
                                           Hashmap *request_headers);
char *http_response_to_buffer(const struct http_response *response);
void http_response_destroy(struct http_response *response);

enum HTTP_RESPONSE_OPTIONS {
  HTTP_VERSION,
  HEADERS,
  BODY,
};

struct http_response_builder;

struct http_response_builder *
create_http_response_builder(enum http_status status_code);
void destroy_http_response_builder(struct http_response_builder *builder);
void http_response_builder_option(struct http_response_builder *builder,
                                  int option, ...);
struct http_response *
http_response_builder_construct(struct http_response_builder *builder);

#endif
