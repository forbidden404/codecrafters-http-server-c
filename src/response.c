// Response
// status-line    = HTTP-version SP status-code SP [ reason-phrase ]
// status-code    = 3DIGIT
// reason-phrase  = 1*( HTAB / SP / VCHAR / obs-text )
//
// field-line     = field-name ":" OWS field-value OWS

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "response.h"

#define BUFFER_SIZE 1024
#define MESSAGE_SIZE 512

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

struct http_response *create_empty_http_1_1_message(enum http_status status,
                                                    const char *reason_phrase) {
  return create_http_1_1_message(status, reason_phrase, NULL);
}

struct http_response *create_http_1_1_message(enum http_status status,
                                              const char *reason_phrase,
                                              const char *field_line) {
  return create_message("HTTP/1.1", status, reason_phrase, field_line);
}

struct http_response *create_message(const char *http_version,
                                     enum http_status status,
                                     const char *reason_phrase,
                                     const char *field_line) {
  if (http_version == NULL) {
    return NULL;
  }

  struct http_response *response = calloc(1, sizeof(*response));

  response->http_version = strdup(http_version);
  response->status = status;

  if (reason_phrase != NULL) {
    response->reason_phrase = strdup(reason_phrase);
  }

  if (field_line != NULL) {
    response->field_line = strdup(field_line);
  }

  return response;
}

struct http_response *create_plain_message(const char *message) {
  size_t message_len = strlen(message);

  const char *content_type = "Content-Type: text/plain\r\n";
  size_t content_type_len = strlen(content_type);
  char *content_length = calloc(MESSAGE_SIZE, sizeof(char));

  snprintf(content_length, MESSAGE_SIZE, "Content-Length: %lu\r\n\r\n",
           message_len);
  size_t content_length_len = strlen(content_length);

  size_t total_len = message_len + content_type_len + content_length_len;
  char *buffer = calloc(total_len, sizeof(char));

  char *ptr = buffer;

  ptr += sprintf(ptr, "%s", content_type);
  ptr += sprintf(ptr, "%s", content_length);
  ptr += sprintf(ptr, "%s", message);

  return create_http_1_1_message(HTTP_OK, "OK", buffer);
}
