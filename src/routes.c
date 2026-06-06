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

const size_t routes_len = 4;

char *headers_body_to_field_line(Hashmap *headers, char *buffer,
                                 size_t buffer_size);

struct http_response *route_root(const struct http_request *request,
                                 Hashmap *params) {
  if (strncmp(request->method, "GET", 3) != 0) {
    return NULL;
  }

  return create_empty_http_1_1_message(HTTP_OK, "OK");
}

struct http_response *route_echo(const struct http_request *request,
                                 Hashmap *params) {
  bstring result = Hashmap_get(params, bfromcstr("param"));
  return create_plain_message(bdata((bstring)result));
}

struct http_response *route_user_agent(const struct http_request *request,
                                       Hashmap *params) {
  bstring result = Hashmap_get(request->headers, bfromcstr("User-Agent"));
  if (result == NULL) {
    return NULL;
  }

  return create_plain_message(bstr2cstr(result, 0));
}

struct http_response *get_files(const struct http_request *request,
                                Hashmap *params) {
  bstring directory = Hashmap_get(params, bfromcstr("directory"));
  bstring filename = Hashmap_get(params, bfromcstr("param"));
  if (directory == NULL || filename == NULL) {
    return create_empty_http_1_1_message(HTTP_BAD_REQUEST, "Bad Request");
  }

  bconcat(directory, filename);
  char *path = bstr2cstr(directory, 0);

  FILE *file = fopen(path, "r");

  // Check if file exists
  if (file == NULL) {
    return create_empty_http_1_1_message(HTTP_NOT_FOUND, "Not Found");
  }

  fseek(file, 0, SEEK_END);

  long length = ftell(file);

  fseek(file, 0, SEEK_SET);

  char *buffer = calloc(length + 1, sizeof(char));
  if (buffer) {
    fread(buffer, 1, length, file);
  }

  fclose(file);

  const char *content_type = "Content-Type: application/octet-stream\r\n";
  size_t content_type_len = strlen(content_type);
  char *content_length = calloc(MESSAGE_SIZE, sizeof(char));

  snprintf(content_length, MESSAGE_SIZE, "Content-Length: %lu\r\n\r\n", length);
  size_t content_length_len = strlen(content_length);

  size_t total_len = length + content_type_len + content_length_len;
  char *message = calloc(total_len, sizeof(char));

  char *ptr = message;

  ptr += sprintf(ptr, "%s", content_type);
  ptr += sprintf(ptr, "%s", content_length);
  ptr += sprintf(ptr, "%s", buffer);

  return create_http_1_1_message(HTTP_OK, "OK", message);
}

struct http_response *post_files(const struct http_request *request,
                                 Hashmap *params) {
  bstring directory = Hashmap_get(params, bfromcstr("directory"));
  bstring filename = Hashmap_get(params, bfromcstr("param"));
  bstring content_length_str =
      Hashmap_get(request->headers, bfromcstr("Content-Length"));
  long content_length;

  if (directory == NULL || filename == NULL ||
      parse_long(bdata((bstring)content_length_str), &content_length) != 0) {
    return create_empty_http_1_1_message(HTTP_BAD_REQUEST, "Bad Request");
  }

  bconcat(directory, filename);
  char *path = bstr2cstr(directory, 0);

  FILE *file = fopen(path, "w");

  // Check if file exists
  if (file == NULL) {
    // Should probably be a 500
    return create_empty_http_1_1_message(HTTP_NOT_FOUND, "Not Found");
  }

  fwrite((char *)(request->data + 2), content_length + 1, 1, file);

  fclose(file);

  return create_empty_http_1_1_message(HTTP_CREATED, "Created");
}

struct http_response *route_files(const struct http_request *request,
                                  Hashmap *params) {
  if (params == NULL) {
    return NULL;
  }

  if (strcmp(request->method, "GET") == 0) {
    return get_files(request, params);
  } else if (strcmp(request->method, "POST") == 0) {
    return post_files(request, params);
  }

  return NULL;
}
