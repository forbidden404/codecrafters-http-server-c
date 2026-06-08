#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bstrlib.h"
#include "compression.h"
#include "hashmap.h"
#include "request.h"
#include "response.h"

#define BUFFER_SIZE 1024
#define MESSAGE_SIZE 512

struct http_response {
  char *http_version;
  enum http_status status;
  char *reason_phrase;

  unsigned char *field_line;
  size_t field_line_len;

  size_t total_size;
};

size_t http_response_size(const struct http_response *response) {
  return response->total_size;
}

unsigned char *http_response_to_buffer(const struct http_response *response) {
  if (response == NULL) {
    return NULL;
  }

  unsigned char *buffer = calloc(response->total_size, sizeof(*buffer));

  size_t http_version_len = strlen(response->http_version);

  int offset = 0;
  for (; offset < http_version_len; offset++) {
    buffer[offset] = response->http_version[offset];
  }

  buffer[offset++] = ' ';

  size_t status_code_len = 4;
  char *status_code_str = calloc(status_code_len, sizeof(char));
  snprintf(status_code_str, status_code_len, "%d", response->status);

  status_code_len += offset - 1;
  for (int i = 0; offset < status_code_len; offset++, i++) {
    buffer[offset] = status_code_str[i];
  }

  buffer[offset++] = ' ';

  size_t reason_phrase_len = 0;
  if (response->reason_phrase) {
    reason_phrase_len = strlen(response->reason_phrase);
  }
  reason_phrase_len += offset;
  for (int i = 0; offset < reason_phrase_len; offset++, i++) {
    buffer[offset] = response->reason_phrase[i];
  }

  buffer[offset++] = '\r';
  buffer[offset++] = '\n';

  size_t field_line_len = offset + response->field_line_len;
  for (int i = 0; offset < field_line_len; offset++, i++) {
    buffer[offset] = response->field_line[i];
  }

  buffer[offset++] = '\r';
  buffer[offset++] = '\n';

  return buffer;
}

void http_response_destroy(struct http_response *request) {
  if (request == NULL)
    return;

  if (request->reason_phrase)
    free(request->reason_phrase);

  if (request->field_line)
    free(request->field_line);

  if (request->http_version)
    free(request->http_version);

  free(request);
}

struct http_response_builder {
  char *http_version;
  enum http_status status_code;

  Hashmap *headers;

  unsigned char *data;
};

struct http_response_builder *
create_http_response_builder(enum http_status status_code) {
  struct http_response_builder *builder = calloc(1, sizeof(*builder));

  builder->http_version = strdup("HTTP/1.1");
  builder->status_code = status_code;

  return builder;
}

void destroy_http_response_builder(struct http_response_builder *builder) {
  if (!builder) {
    return;
  }

  if (builder->http_version) {
    free(builder->http_version);
  }

  if (builder->headers) {
    Hashmap_destroy(builder->headers);
  }

  if (builder->data) {
    free(builder->data);
  }

  free(builder);
}

void http_response_builder_option(struct http_response_builder *builder,
                                  int option, ...) {
  va_list args;

  long content_length = 0;

  va_start(args, option);
  switch (option) {
  case HTTP_VERSION:
    if (builder->http_version) {
      free(builder->http_version);
    }
    builder->http_version = strdup(va_arg(args, char *));
    break;
  case HEADERS:
    builder->headers = va_arg(args, Hashmap *);
    break;
  case -HEADERS:
    builder->headers = NULL;
    break;
  case BODY:
    builder->data = (unsigned char *)strdup(va_arg(args, char *));
    break;
  case -BODY:
    builder->data = NULL;
    break;
  case COMPRESSION_GZIP:
    if (builder->data && builder->headers) {
      bstring content_length_key = bfromcstr("Content-Length");
      bstring content_length_str =
          Hashmap_get(builder->headers, content_length_key);
      if (content_length_str) {
        parse_long(bdata((bstring)content_length_str), &content_length);

        unsigned char *output;
        size_t output_len = 0;

        if (gzip_compress((char *)builder->data, content_length, &output,
                          &output_len) != 0) {
        }

        char *output_len_str = calloc(BUFFER_SIZE, sizeof(char));
        snprintf(output_len_str, BUFFER_SIZE, "%lu", output_len);
        Hashmap_delete(builder->headers, content_length_key);
        Hashmap_set(builder->headers, content_length_key,
                    bfromcstr(output_len_str));

        Hashmap_set(builder->headers, bfromcstr("Content-Encoding"),
                    bfromcstr("gzip"));

        memcpy(builder->data, output, output_len);
      }
    }
    break;
  }

  va_end(args);
}

void http_response_builder_plain_message(struct http_response_builder *builder,
                                         const char *message) {
  Hashmap *headers = Hashmap_create(NULL, NULL);
  Hashmap_set(headers, bfromcstr("Content-Type"), bfromcstr("text/plain"));
  char *content_length = calloc(MESSAGE_SIZE, sizeof(char));

  snprintf(content_length, MESSAGE_SIZE, "%lu", strlen(message));
  Hashmap_set(headers, bfromcstr("Content-Length"), bfromcstr(content_length));
  free(content_length);
  http_response_builder_option(builder, HEADERS, headers);
  http_response_builder_option(builder, BODY, message);
}

char *map_to_buffer = NULL;
size_t map_to_buffer_size = 0;
int traverse_hashmap(HashmapNode *node) {
  char *key = bdata((bstring)node->key);
  size_t key_len = blength((bstring)node->key);
  char *value = bdata((bstring)node->data);
  size_t value_len = blength((bstring)node->data);

  size_t total_size = key_len + 2 /* :sp */ + value_len + 2; /* \r\n */
  map_to_buffer = realloc(map_to_buffer, total_size + map_to_buffer_size + 1);

  if (!map_to_buffer) {
    return -1;
  }

  if (map_to_buffer_size == 0) {
    map_to_buffer[0] = 0;
  }

  map_to_buffer_size += total_size + 1;

  char *buffer = (char *)malloc((1 + total_size) * sizeof(char));
  if (!buffer) {
    return -1;
  }

  sprintf(buffer, "%s: %s\r\n", key, value);
  strncat(map_to_buffer, buffer, total_size);
  map_to_buffer[map_to_buffer_size - 1] = 0;

  free(buffer);

  return 0;
}

struct http_response *
http_response_builder_construct(struct http_response_builder *builder) {
  struct http_response *response = calloc(1, sizeof(*response));

  response->http_version = strdup(builder->http_version);
  response->total_size = strlen(builder->http_version) + 1;
  response->status = builder->status_code;
  response->total_size += 4;

  switch (builder->status_code) {
  case HTTP_OK:
    response->reason_phrase = strdup("OK");
    break;
  case HTTP_CREATED:
    response->reason_phrase = strdup("Created");
    break;
  case HTTP_BAD_REQUEST:
    response->reason_phrase = strdup("Bad Request");
    break;
  case HTTP_NOT_FOUND:
    response->reason_phrase = strdup("Not Found");
    break;
  case HTTP_INTERNAL_SERVER_ERROR:
    response->reason_phrase = strdup("Internal Server Error");
    break;
  }
  response->total_size += strlen(response->reason_phrase) + 2;

  if (builder->headers) {
    map_to_buffer_size = 0;
    if (map_to_buffer) {
      free(map_to_buffer);
      map_to_buffer = NULL;
    }
    Hashmap_traverse(builder->headers, traverse_hashmap);

    // Add empty line
    map_to_buffer_size += 3;
    map_to_buffer = realloc(map_to_buffer, map_to_buffer_size);
    strncat(map_to_buffer, "\r\n", map_to_buffer_size);

    response->total_size += map_to_buffer_size - 3;
    response->field_line_len = map_to_buffer_size;
    response->field_line = (unsigned char *)strdup(map_to_buffer);
  }

  if (!builder->data || !builder->headers) {
    response->total_size += 2;
    return response;
  }

  bstring content_length_str =
      Hashmap_get(builder->headers, bfromcstr("Content-Length"));
  if (!content_length_str) {
  } else {
    long content_length;
    parse_long(bdata((bstring)content_length_str), &content_length);
    response->total_size += content_length;
    response->field_line_len += content_length;

    size_t current_len = strlen((char *)response->field_line);
    size_t new_len = current_len + content_length + 1;
    response->field_line = realloc(response->field_line, new_len);

    for (int i = 0; (i + current_len) < new_len; i++) {
      response->field_line[i + current_len] = builder->data[i];
    }
  }

  return response;
}
