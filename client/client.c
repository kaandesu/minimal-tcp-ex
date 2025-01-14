#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
  ADDRESS = 1,
  PORT,
};

void check(int e, const char *context) {
  if (e < 0) {
    fprintf(stderr, "%s failed: %s\n", context, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void *receive_messages(void *arg) {
  int client_fd = *(int *)arg;
  char buffer[1024];
  while (1) {
    ssize_t size = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (size <= 0)
      break;
    buffer[size] = '\0';
    printf(">> %s", buffer);
  }
  return NULL;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <addr> <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int client_fd = socket(AF_INET, SOCK_STREAM, 0);
  check(client_fd, "socket");

  struct sockaddr_in server_info = {0};
  server_info.sin_family = AF_INET;
  check(inet_pton(AF_INET, argv[ADDRESS], &server_info.sin_addr), "inet_pton");
  server_info.sin_port = htons(atoi(argv[PORT]));

  check(
      connect(client_fd, (struct sockaddr *)&server_info, sizeof(server_info)),
      "connect");

  pthread_t recv_thread;
  check(pthread_create(&recv_thread, NULL, receive_messages, &client_fd),
        "pthread_create");

  char msg[256];
  while (fgets(msg, sizeof(msg), stdin)) {
    if (strcmp(msg, "exit\n") == 0)
      break;
    ssize_t size = send(client_fd, msg, strlen(msg), 0);
    if (size < 0) {
      fprintf(stderr, "Send failed: %s\n", strerror(errno));
      break;
    }
  }

  pthread_cancel(recv_thread);
  pthread_join(recv_thread, NULL);
  close(client_fd);
  return 0;
}
