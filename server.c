#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 7000
#define ADDRESS INADDR_ANY
#define DOMAIN AF_INET
#define SOCKET_TYPE SOCK_STREAM
#define REQUEST_BUFFER_SIZE 256

int initServer()
{
  int server_fd = socket(DOMAIN, SOCKET_TYPE, 0);

  struct sockaddr_in *server_address;

  if (server_fd < 0)
  {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  server_address->sin_family = DOMAIN;
  server_address->sin_addr.s_addr = htonl(ADDRESS);
  server_address->sin_port = htons(PORT);

  socklen_t server_socklen = sizeof(*server_address);

  if (bind(server_fd, (const struct sockaddr *)server_address, server_socklen) < 0)
  {
    perror("Error! Bind has failed\n");
    exit(EXIT_FAILURE);
  }

  return server_fd;
}

int main(int argc, char const *argv[])
{
  int server_fd = initServer();

  if (listen(server_fd, 10) < 0)
  {
    printf("Error! Can't listen\n");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in *client_sockaddr = malloc(sizeof(struct sockaddr_in));
  socklen_t client_socklen = sizeof(*client_sockaddr);

  char *request_buffer = malloc(REQUEST_BUFFER_SIZE * sizeof(char));
  char *response_buffer = NULL;

  while (1)
  {
    int client_socket_fd = accept(server_fd, (struct sockaddr *)&client_sockaddr, &client_socklen);

    int childPID = fork();
    if (childPID == 0)
    {
      close(server_fd);

      if (client_socket_fd == -1)
      {
        perror("Unable to create client file descriptor.");
        exit(EXIT_FAILURE);
      }

      char *buff = "hello from server\0";
      send(client_socket_fd, buff, strlen(buff), 0);

      recv(client_socket_fd, buff, strlen(buff), 0);
      printf("%s\n", buff);
    }
  }
  

  return 0;
}
