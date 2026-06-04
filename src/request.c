#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bstrlib.h"
#include "hashmap.h"
#include "request.h"

int parse_long(const char *str, long *out) {
  if (*str == '\0') {
    return -1;
  }

  char *end;
  errno = 0;
  long value = strtol(str, &end, 0);
  if (*end != '\0') {
    return -1;
  }

  if (errno == ERANGE) {
    return -1;
  }

  *out = value;
  return 0;
}

size_t parse_headers(char *buffer, Hashmap *headers, char **end_ptr) {
  size_t count = 0;
  char *current = buffer;

  while (*current != '\0') {
    char *line_end = strstr(current, "\r\n");

    if (line_end == NULL) {
      break;
    }

    size_t line_len = line_end - current;

    // empty line
    if (line_len == 0) {
      break;
    }

    const char *colon = memchr(current, ':', line_len);

    if (colon != NULL) {
      size_t key_len = colon - current;

      const char *value_start = colon + 1;

      while (*value_start == ' ') {
        value_start++;
      }

      size_t value_len = line_end - value_start;

      char key[256];
      char value[256];
      snprintf(key, 256, "%.*s", (int)key_len, current);

      snprintf(value, 256, "%.*s", (int)value_len, value_start);

      Hashmap_set(headers, cstr2bstr(key), cstr2bstr(value));

      count++;
    }

    current = line_end + 2;
  }

  *end_ptr = current;

  return count;
}

struct http_request *http_request_from_buffer(const char *buffer) {
  if (buffer == NULL) {
    return NULL;
  }
  char *tmp = malloc(strlen(buffer));
  memcpy(tmp, buffer, strlen(buffer));

  struct http_request *request = calloc(1, sizeof(*request));

  char *method = strtok(tmp, " ");
  char *request_target = strtok(NULL, " ");
  char *http_version = strtok(NULL, "\r\n");

  request->method = strdup(method);
  request->request_target = strdup(request_target);
  request->http_version = strdup(http_version);

  char *headers_to_parse = strtok(NULL, "\0");

  request->headers = Hashmap_create(NULL, NULL);
  char *body;
  parse_headers(headers_to_parse, request->headers, &body);

  char *content_length_str =
      bstr2cstr(Hashmap_get(request->headers, bfromcstr("Content-Length")), 0);
  long content_length;

  if (content_length_str != NULL &&
      parse_long(content_length_str, &content_length) == 0) {
    request->data = calloc(content_length + 1, sizeof(char));
    memcpy(request->data, strtok(body, "\0"), content_length);
  }

  return request;
}

void http_request_destroy(struct http_request *request) {
  if (request == NULL)
    return;

  if (request->method)
    free(request->method);

  if (request->request_target)
    free(request->request_target);

  if (request->http_version)
    free(request->http_version);

  if (request->headers)
    Hashmap_destroy(request->headers);

  if (request->data)
    free(request->data);

  free(request);
}
