// Response
// status-line    = HTTP-version SP status-code SP [ reason-phrase ]
// status-code    = 3DIGIT
// reason-phrase  = 1*( HTAB / SP / VCHAR / obs-text )
//
// field-line     = field-name ":" OWS field-value OWS

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "bstrlib.h"
#include "hashmap.h"
#include "request.h"
#include "response.h"

#define BUFFER_SIZE 1024
#define MESSAGE_SIZE 512

int gzip_compress(const char *input, size_t input_len, unsigned char **output,
                  size_t *output_len) {
  z_stream zs = {0};

  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                   15 + 16, /* gzip header */
                   8, Z_DEFAULT_STRATEGY) != Z_OK) {
    return -1;
  }

  size_t buffer_size = compressBound(input_len);
  *output = malloc(buffer_size);

  zs.next_in = (Bytef *)input;
  zs.avail_in = input_len;

  zs.next_out = *output;
  zs.avail_out = buffer_size;

  int ret = deflate(&zs, Z_FINISH);

  if (ret != Z_STREAM_END) {
    free(*output);
    deflateEnd(&zs);
    return -1;
  }

  *output_len = zs.total_out;

  deflateEnd(&zs);

  return 0;
}

struct http_response {
  char *http_version;
  enum http_status status;
  char *reason_phrase;

  char *field_line;
};

char *http_response_to_buffer(const struct http_response *response) {
  if (response == NULL) {
    return NULL;
  }

  char *buffer = calloc(BUFFER_SIZE, sizeof(char));
  sprintf(buffer, "%s %d %s\r\n%s\r\n", response->http_version,
          response->status,
          response->reason_phrase == NULL ? "" : response->reason_phrase,
          response->field_line == NULL ? "" : response->field_line);

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

struct http_response *create_plain_message(const char *message,
                                           Hashmap *request_headers) {
  struct http_response_builder *builder = create_http_response_builder(HTTP_OK);

  Hashmap *headers = Hashmap_create(NULL, NULL);
  Hashmap_set(headers, bfromcstr("Content-Type"), bfromcstr("text/plain"));
  char *content_length = calloc(MESSAGE_SIZE, sizeof(char));

  int is_encoding = 0;
  if (request_headers) {
    bstring accept_encoding =
        Hashmap_get(request_headers, bfromcstr("Accept-Encoding"));
    if (accept_encoding) {
      char *accept_encoding_str = bdata((bstring)accept_encoding);
      char *encoding = strtok(accept_encoding_str, " ,\0");

      while (encoding != NULL && strcmp(encoding, "gzip") != 0) {
        encoding = strtok(NULL, " ,\0");
      }

      if (encoding && strcmp(encoding, "gzip") == 0) {
        is_encoding = 1;
        Hashmap_set(headers, bfromcstr("Content-Encoding"), bfromcstr("gzip"));
      }
    }
  }

  if (is_encoding) {
    unsigned char *compressed;
    size_t compressed_len;

    if (gzip_compress(message, strlen(message), &compressed, &compressed_len)) {
      snprintf(content_length, MESSAGE_SIZE, "%lu", compressed_len);
      Hashmap_set(headers, bfromcstr("Content-Length"),
                  bfromcstr(content_length));
      free(content_length);
      http_response_builder_option(builder, HEADERS, headers);
      http_response_builder_option(builder, BODY, compressed);
    }
  } else {
    snprintf(content_length, MESSAGE_SIZE, "%lu", strlen(message));
    Hashmap_set(headers, bfromcstr("Content-Length"),
                bfromcstr(content_length));
    free(content_length);
    http_response_builder_option(builder, HEADERS, headers);
    http_response_builder_option(builder, BODY, message);
  }

  return http_response_builder_construct(builder);
}

struct http_response_builder {
  char *http_version;
  enum http_status status_code;

  Hashmap *headers;

  char *data;
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
    long content_length;
    parse_long(bdata((bstring)Hashmap_get(builder->headers,
                                          bfromcstr("Content-Length"))),
               &content_length);
    memcpy(builder->data, va_arg(args, void *), content_length);
    printf("Builder data: ");
    for (int i = 0; i < content_length; i++) {
      printf("%02X ", builder->data[i]);
    }
    printf("\n");
    break;
  case -BODY:
    builder->data = NULL;
    break;
  }

  va_end(args);
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
  response->status = builder->status_code;

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

    response->field_line = strdup(map_to_buffer);
  }

  if (!builder->data || !builder->headers) {
    return response;
  }

  // if content-length is not set, we will try to write until we see the
  // first zero value in `data` and set the length according to it
  bstring content_length_str =
      Hashmap_get(builder->headers, bfromcstr("Content-Length"));
  if (!content_length_str) {
  } else {
    long content_length;
    parse_long(bdata((bstring)content_length_str), &content_length);
    size_t new_len = strlen(response->field_line) + content_length + 1;
    response->field_line = realloc(response->field_line, new_len);
    strncat(response->field_line, builder->data, new_len);
  }

  return response;
}
