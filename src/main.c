#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

const char *ok_message = "HTTP/1.1 200 OK\r\n\r\n";
const char *bad_request_message = "HTTP/1.1 401 Bad Request\r\n\r\n";
const char *not_found_message = "HTTP/1.1 404 Not Found\r\n\r\n";

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

int root(int socket_fd, const char *buffer, const char *http_method,
         const char *http_request_target) {
  if (strncmp(http_request_target, "/", strlen(http_request_target)) != 0) {
    return 0;
  }

  if (strncmp(http_method, "GET", 3) != 0) {
    return 0;
  }

  log_send(socket_fd, ok_message, strlen(ok_message), 0);
  return 1;
}

int echo(int socket_fd, const char *buffer, const char *http_method,
         const char *http_request_target) {
  const char *echo_str = "/echo/";
  size_t echo_len = strlen(echo_str);
  char message[64] = {0};
  const char *message_fmt = "/echo/%s";

  if (strncmp(http_request_target, echo_str, echo_len) != 0) {
    return 0;
  }

  if (strncmp(http_method, "GET", 3) != 0) {
    return 0;
  }

  if (sscanf(http_request_target, message_fmt, message) != 1) {
    return 0;
  }

  char *response = generate_plain_response(message);
  log_send(socket_fd, response, strlen(response), 0);

  return 1;
}

int user_agent(int socket_fd, const char *buffer, const char *http_method,
               const char *http_request_target) {
  const char *user_agent_str = "/user-agent";
  size_t user_agent_len = strlen(user_agent_str);

  if (strncmp(http_request_target, user_agent_str, user_agent_len) != 0) {
    return 0;
  }

  const char *header_fmt = "%*s %*s %*s\r\nHost: %*s\r\nUser-Agent: %s\r\n";
  char user_agent_value[64] = {0};

  if (sscanf(buffer, header_fmt, user_agent_value) != 1) {
    return 0;
  }

  char *response = generate_plain_response(user_agent_value);
  log_send(socket_fd, response, strlen(response), 0);

  return 1;
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

  int socket_fd = 0;
  while (1) {
    socket_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                       (socklen_t *)&client_addr_len);
    if (socket_fd == -1) {
      return 1;
    }
    printf("Client connected\n");

    if (!fork()) {
      close(server_fd);

      char *buffer = malloc(1024 * sizeof(char));
      while (recv(socket_fd, buffer, 1024, 0) > 0) {
        printf("Request received: %s \n", buffer);
        const char *format = "%s %s";
        char http_method[16] = {0};
        char http_request_target[64] = {0};

        if (sscanf(buffer, format, http_method, http_request_target) == 2) {
          printf("Request parsed => method : '%s' \t request-target : '%s' \n",
                 http_method, http_request_target);

          int (*handlers[])(int, const char *, const char *,
                            const char *) = {root, echo, user_agent};
          size_t handlers_len = 3;
          int index = 0;
          int has_been_handled = 0;

          for (; index < handlers_len && has_been_handled == 0; index++) {
            has_been_handled = handlers[index](socket_fd, buffer, http_method,
                                               http_request_target);
          }

          if (!has_been_handled) {
            log_send(socket_fd, not_found_message, strlen(not_found_message),
                     0);
          }

        } else {
          log_send(socket_fd, bad_request_message, strlen(bad_request_message),
                   0);
        }
      }

      close(socket_fd);
      exit(0);
    }

    close(socket_fd);
  }

  close(server_fd);

  return 0;
}
