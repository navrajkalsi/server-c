#include "../include/client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int handle_client(Client *client) {
  if (!client) {
    errno = EFAULT;
    return -1;
  }

  char *res = "hello world.";
  write(client->fd, res, strlen(res));

  return 0;
}

void print_request(Client *client) {
  if (!client)
    return;

  // Even if the client has an ip4 address, the client_address is filled with
  // a ip6 mapped ip4 address
  // Therefore, no need for dealing with both here individually
  char ipstr[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)(client->address))->sin6_addr),
            ipstr, sizeof ipstr);

  printf("Client: %s\n", ipstr);
}
