#include <arpa/inet.h>
#include <netdb.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>

int main() {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *response;
  int result = getaddrinfo("navthinks.vip", "http", &hints, &response);
  printf("getaddrinfo result: %d\n", result);

  printf("response: %d\n", response->ai_addr->sa_family);
  struct sockaddr_in *ipv4 = (struct sockaddr_in *)response->ai_addr;
  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(ipv4->sin_addr), ip, sizeof(ip));
  printf("IPv4 address: %s\n", ip);
  printf("Port: %d\n", ntohs(ipv4->sin_port));

  return 0;
}
