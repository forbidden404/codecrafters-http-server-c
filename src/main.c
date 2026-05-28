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
  const char *bad_request_message = "HTTP/1.1 401 BAD REQUEST\r\n\r\n";
  const char *not_found_message = "HTTP/1.1 404 NOT FOUND\r\n\r\n";

  char *buffer = malloc(1024 * sizeof(char));
  while (recv(accepted_socket, buffer, 1024, 0) != 0) {
    printf("Request received: %s \n", buffer);
    const char *format = "%s %s";
    const char http_method[16] = {0};
    const char http_request_target[64] = {0};

    int parsing_result =
        sscanf(buffer, format, http_method, http_request_target);

    if (parsing_result != 2) {
      printf("Request parsed => method : %s \t request-target : %s \n",
             http_method, http_request_target);
      if (strlen(http_request_target) == 1 && http_request_target[0] == '/') {
        log_send(accepted_socket, ok_message, strlen(ok_message), 0);
      } else {
        log_send(accepted_socket, not_found_message, strlen(not_found_message),
                 0);
      }
    } else {
      printf("Parsing result: %d \n", parsing_result);
      log_send(accepted_socket, bad_request_message,
               strlen(bad_request_message), 0);
    }
  }

  close(server_fd);

  return 0;
}
