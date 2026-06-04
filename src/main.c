#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "request.h"
#include "response.h"
#include "routes.h"

#define HTTP_BUFFER_SIZE 1024

static void log_send(int socket_fd, const void *buffer, size_t buffer_size) {
  if (send(socket_fd, buffer, buffer_size, 0) == -1) {
    printf("Message sending failed: %d: %s \n", errno, strerror(errno));
  }
}

int route_matches(const struct route *route, const char *path);

void send_bad_request_message(int socket_fd);
void send_not_found_message(int socket_fd);

int main() {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

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

  const int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  client_addr_len = sizeof(client_addr);

  int socket_fd = 0;
  while (1) {
    socket_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                       (socklen_t *)&client_addr_len);
    if (socket_fd == -1) {
      printf("Accept failed: %s \n", strerror(errno));
      return 1;
    }

    if (!fork()) {
      close(server_fd);

      char *buffer = malloc(HTTP_BUFFER_SIZE * sizeof(char));
      while (recv(socket_fd, buffer, HTTP_BUFFER_SIZE, 0) > 0) {
        struct http_request *request = http_request_from_buffer(buffer);
        if (request == NULL) {
          send_bad_request_message(socket_fd);
          break;
        }

        int index = 0;
        for (; index < routes_len; index++) {
          if (route_matches(&routes[index], request->request_target)) {
            char *param = NULL;
            if (routes[index].type == ROUTE_PARAM) {
              size_t len = strlen(routes[index].path);
              param = request->request_target + len;
            }

            struct http_response *response =
                routes[index].handler(request, param);

            if (response == NULL) {
              continue;
            }

            char *response_buffer = http_response_to_buffer(response);

            log_send(socket_fd, response_buffer, strlen(response_buffer));

            http_response_destroy(response);
            free(response_buffer);

            break;
          }
        }

        if (index == routes_len) {
          send_not_found_message(socket_fd);
        }

        http_request_destroy(request);
      }

      close(socket_fd);
      exit(0);
    }

    close(socket_fd);
  }

  close(server_fd);

  return 0;
}

int route_matches(const struct route *route, const char *path) {
  switch (route->type) {
  case ROUTE_EXACT:
    return strcmp(path, route->path) == 0;
  case ROUTE_PARAM:
    return strncmp(path, route->path, strlen(route->path)) == 0;
  }

  return 0;
}

void send_bad_request_message(int socket_fd) {
  struct http_response *response =
      create_empty_http_1_1_message(HTTP_BAD_REQUEST, "Bad Request");
  char *bad_request_message = http_response_to_buffer(response);

  log_send(socket_fd, bad_request_message, strlen(bad_request_message));

  http_response_destroy(response);
  free(bad_request_message);
}

void send_not_found_message(int socket_fd) {
  struct http_response *response =
      create_empty_http_1_1_message(HTTP_NOT_FOUND, "Not Found");
  char *not_found_message = http_response_to_buffer(response);

  log_send(socket_fd, not_found_message, strlen(not_found_message));

  http_response_destroy(response);
  free(not_found_message);
}
