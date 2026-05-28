#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void log_send(int socket_fd, const void *buffer, size_t buffer_size,
              int flags) {
  if (send(socket_fd, buffer, buffer_size, flags) == -1) {
    printf("Message sending failed: %d: %s \n", errno, strerror(errno));
  }
}

char *generate_plain_response(const char *message) {
  size_t message_len = strlen(message);

  const char *status_line = "HTTP/1.1 200 OK\r\n";
  size_t status_line_len = strlen(status_line);

  const char *content_type = "Content-Type: text/plain\r\n";
  size_t content_type_len = strlen(content_type);
  char *content_length = calloc(64, sizeof(char));

  snprintf(content_length, 64, "Content-Length: %lu\r\n\r\n", message_len);
  size_t content_length_len = strlen(content_length);

  size_t total_len =
      message_len + status_line_len + content_type_len + content_length_len;
  char *response = calloc(total_len, sizeof(char));

  strncat(response, status_line, status_line_len);
  strncat(response, content_type, content_type_len);
  strncat(response, content_length, content_length_len);
  strncat(response, message, message_len);

  return response;
}

int main() {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // You can use print statements as follows for debugging, they'll be visible
  // when running tests.
  printf("Logs from your program will appear here!\n");

  int server_fd, client_addr_len;
  struct sockaddr_in client_addr;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(4221),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  printf("Waiting for a client to connect...\n");
  client_addr_len = sizeof(client_addr);

  int accepted_socket = accept(server_fd, (struct sockaddr *)&client_addr,
                               (socklen_t *)&client_addr_len);
  printf("Client connected\n");

  const char *ok_message = "HTTP/1.1 200 OK\r\n\r\n";
  const char *bad_request_message = "HTTP/1.1 401 Bad Request\r\n\r\n";
  const char *not_found_message = "HTTP/1.1 404 Not Found\r\n\r\n";

  char *buffer = malloc(1024 * sizeof(char));
  while (recv(accepted_socket, buffer, 1024, 0) != 0) {
    printf("Request received: %s \n", buffer);
    const char *format = "%s %s";
    char http_method[16] = {0};
    char http_request_target[64] = {0};

    if (sscanf(buffer, format, http_method, http_request_target) == 2) {
      printf("Request parsed => method : '%s' \t request-target : '%s' \n",
             http_method, http_request_target);
      if (strlen(http_request_target) == 1 && http_request_target[0] == '/') {
        log_send(accepted_socket, ok_message, strlen(ok_message), 0);
      } else {
        const char *echo_str = "/echo/";
        size_t echo_len = strlen(echo_str);
        char message[64] = {0};
        const char *message_fmt = "/echo/%s";

        if (strncmp(http_request_target, echo_str, echo_len) == 0 &&
            sscanf(http_request_target, message_fmt, message) == 1) {
          char *response = generate_plain_response(message);
          printf("Message to send: %s\n", response);
          log_send(accepted_socket, response, strlen(response), 0);
        } else {
          log_send(accepted_socket, not_found_message,
                   strlen(not_found_message), 0);
        }
      }
    } else {
      log_send(accepted_socket, bad_request_message,
               strlen(bad_request_message), 0);
    }
  }

  close(server_fd);

  return 0;
}
