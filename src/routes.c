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

void add_connection_close_if_needed(const struct http_request *request,
                                    struct http_response_builder *builder) {
  bstring connection = Hashmap_get_cstr(request->headers, "Connection");
  if (connection && biseqcstr(connection, "close")) {
    http_response_builder_option(builder, CLOSE);
  }
}

struct http_response *route_root(const struct http_request *request,
                                 Hashmap *params) {
  if (strncmp(request->method, "GET", 3) != 0) {
    return NULL;
  }

  struct http_response_builder *builder = create_http_response_builder(HTTP_OK);
  add_connection_close_if_needed(request, builder);
  struct http_response *response = http_response_builder_construct(builder);
  destroy_http_response_builder(builder);

  return response;
}

struct http_response *route_echo(const struct http_request *request,
                                 Hashmap *params) {
  bstring result = Hashmap_get_cstr(params, "param");
  struct http_response_builder *builder = create_http_response_builder(HTTP_OK);
  http_response_builder_plain_message(builder, bdata((bstring)result));

  bstring accept_encoding =
      Hashmap_get_cstr(request->headers, "Accept-Encoding");
  if (accept_encoding) {
    char *accept_encoding_str = strdup(bdata((bstring)accept_encoding));
    char *encoding = strtok(accept_encoding_str, " ,\0");

    while (encoding != NULL && strcmp(encoding, "gzip") != 0) {
      encoding = strtok(NULL, " ,\0");
    }

    if (encoding && strcmp(encoding, "gzip") == 0) {
      http_response_builder_option(builder, COMPRESSION_GZIP);
    }

    free(accept_encoding_str);
  }

  add_connection_close_if_needed(request, builder);
  struct http_response *response = http_response_builder_construct(builder);
  destroy_http_response_builder(builder);

  return response;
}

struct http_response *route_user_agent(const struct http_request *request,
                                       Hashmap *params) {
  bstring result = Hashmap_get_cstr(request->headers, "User-Agent");
  if (result == NULL) {
    return NULL;
  }

  struct http_response_builder *builder = create_http_response_builder(HTTP_OK);
  http_response_builder_plain_message(builder, bdata((bstring)result));
  add_connection_close_if_needed(request, builder);
  struct http_response *response = http_response_builder_construct(builder);
  destroy_http_response_builder(builder);

  return response;
}

struct http_response *get_files(const struct http_request *request,
                                Hashmap *params) {
  bstring directory = Hashmap_get_cstr(params, "directory");
  bstring filename = Hashmap_get_cstr(params, "param");
  if (directory == NULL || filename == NULL) {
    struct http_response_builder *builder =
        create_http_response_builder(HTTP_BAD_REQUEST);
    struct http_response *response = http_response_builder_construct(builder);
    destroy_http_response_builder(builder);
    return response;
  }

  bconcat(directory, filename);
  char *path = bstr2cstr(directory, 0);

  FILE *file = fopen(path, "r");

  // Check if file exists
  if (file == NULL) {
    struct http_response_builder *builder =
        create_http_response_builder(HTTP_NOT_FOUND);
    add_connection_close_if_needed(request, builder);
    struct http_response *response = http_response_builder_construct(builder);
    destroy_http_response_builder(builder);
    return response;
  }

  fseek(file, 0, SEEK_END);

  long length = ftell(file);

  fseek(file, 0, SEEK_SET);

  char *buffer = calloc(length + 1, sizeof(char));
  if (buffer) {
    fread(buffer, 1, length, file);
  }

  fclose(file);

  struct http_response_builder *builder = create_http_response_builder(HTTP_OK);

  Hashmap *headers = Hashmap_create(NULL, NULL);
  Hashmap_set(headers, bfromcstr("Content-Type"),
              bfromcstr("application/octet-stream"));
  char *content_length = calloc(MESSAGE_SIZE, sizeof(char));
  snprintf(content_length, MESSAGE_SIZE, "%lu", length);
  Hashmap_set(headers, bfromcstr("Content-Length"), bfromcstr(content_length));

  http_response_builder_option(builder, HEADERS, headers);
  http_response_builder_option(builder, BODY, buffer);

  add_connection_close_if_needed(request, builder);
  struct http_response *response = http_response_builder_construct(builder);
  destroy_http_response_builder(builder);
  return response;
}

struct http_response *post_files(const struct http_request *request,
                                 Hashmap *params) {
  bstring directory = Hashmap_get_cstr(params, "directory");
  bstring filename = Hashmap_get_cstr(params, "param");
  bstring content_length_str =
      Hashmap_get_cstr(request->headers, "Content-Length");
  long content_length;

  if (directory == NULL || filename == NULL || content_length_str == NULL ||
      parse_long(bdata((bstring)content_length_str), &content_length) != 0) {
    struct http_response_builder *builder =
        create_http_response_builder(HTTP_BAD_REQUEST);
    add_connection_close_if_needed(request, builder);
    struct http_response *response = http_response_builder_construct(builder);
    destroy_http_response_builder(builder);
    return response;
  }

  bconcat(directory, filename);
  char *path = bstr2cstr(directory, 0);

  FILE *file = fopen(path, "w");

  // Check if file exists
  if (file == NULL) {
    struct http_response_builder *builder =
        create_http_response_builder(HTTP_INTERNAL_SERVER_ERROR);
    add_connection_close_if_needed(request, builder);
    struct http_response *response = http_response_builder_construct(builder);
    destroy_http_response_builder(builder);
    return response;
  }

  fwrite((char *)request->data, content_length, 1, file);

  fclose(file);

  struct http_response_builder *builder =
      create_http_response_builder(HTTP_CREATED);
  add_connection_close_if_needed(request, builder);
  struct http_response *response = http_response_builder_construct(builder);
  destroy_http_response_builder(builder);
  return response;
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
