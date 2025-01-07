#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/_types/_socklen_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/_types/_u_int8_t.h>
#include <sys/socket.h>
#include <unistd.h>

enum {
  ADDRESS = 1,
  PORT,
};

void check(int e, const char *context) {
  if (e < 0) {
    printf("%s failed: %s\n", context, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void *recieve_messages(void *arg) {
  int client_fd = *(int *)arg;
  char buffer[1024];
  memset(buffer, 0, 1024);
  while (1) {
    ssize_t size = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (size <= 0)
      break;
    buffer[size] = '\0';
    printf(">>%s", buffer);
  }
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  int client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (argc < 2) {
    perror("./client <addr> <port>\n");
  }

  struct sockaddr_in server_info = {
      .sin_family = AF_INET,
      .sin_addr = htonl(atoi(argv[ADDRESS])),
      .sin_port = htons(atoi(argv[PORT])),
  };

  check(connect(client_fd, (const struct sockaddr *)&server_info,
                sizeof(server_info)),
        "connect");

  pthread_t recv_thread;
  pthread_create(&recv_thread, NULL, recieve_messages, &client_fd);

  char msg[256];
  while (1) {
    fgets(msg, sizeof(msg), stdin);
    ssize_t size = send(client_fd, msg, strlen(msg), 0);
    if (size < 0) {
      printf("Send failed: %s\n", strerror(errno));
      break;
    }
  }

  close(client_fd);
}
