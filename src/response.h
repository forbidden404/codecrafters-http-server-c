#ifndef RESPONSE_H
#define RESPONSE_H

enum http_status {
  HTTP_OK = 200,
  HTTP_CREATED = 201,
  HTTP_BAD_REQUEST = 400,
  HTTP_NOT_FOUND = 404,
};

struct http_response {
  char *http_version;
  enum http_status status;
  char *reason_phrase;

  char *field_line;
};

struct http_response *create_message(const char *http_version,
                                     enum http_status status,
                                     const char *reason_phrase,
                                     const char *field_line);
struct http_response *create_http_1_1_message(enum http_status status,
                                              const char *reason_phrase,
                                              const char *field_line);
struct http_response *create_empty_http_1_1_message(enum http_status status,
                                                    const char *reason_phrase);
struct http_response *create_plain_message(const char *message);
char *http_response_to_buffer(const struct http_response *response);
void http_response_destroy(struct http_response *response);

#endif
