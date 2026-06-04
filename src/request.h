#ifndef REQUEST_H
#define REQUEST_H

#include "hashmap.h"

struct http_request {
  char *method;
  char *request_target;
  char *http_version;

  Hashmap *headers;

  void *data;
};

struct http_request *http_request_from_buffer(const char *buffer);
void http_request_destroy(struct http_request *request);

#endif /* REQUEST_H */
