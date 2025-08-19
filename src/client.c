#include "../include/client.h"
#include "../include/request.h"
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int handle_client(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // Temporary buffer to be used for data storage for unknown length types
  // no need for void, only gonna use chars
  char buf[BUF_MAX], *buf_ptr = buf;
  size_t total_read = 0;
  long read_status = 0;
  const char *request_end = "\r\n\r\n";
  char *end_ptr;

  while ((read_status = read(client->fd, buf_ptr, BUF_MAX - total_read - 1)) >
         0) {
    total_read += (size_t)read_status;
    buf_ptr = &buf[total_read];
    buf[total_read] = '\0';

    if ((end_ptr = strstr(buf, request_end)))
      // No need to read more
      break;
  }

  if (read_status != -1 && end_ptr) {
    // Advancing the ptr by 4 chars to get past the request end
    // Then comparing with buf_ptr to see if they are same
    // If same that means there is no body after headers and the total_read is
    // the correct length else change total_read to the length of only the
    // request headers
    end_ptr = &(end_ptr[strlen(request_end)]);
    if (end_ptr != buf_ptr)
      // Discard if there is any body in the request
      // Only supporting GET requests for now
      total_read = (size_t)(end_ptr - buf);
  } else {
    errno = errno ? errno : EMSGSIZE;
    return err("Reading request", true);
  }

  // At this point total_read is the correct len of str
  if (!(client->request.data = (char *)malloc(total_read)))
    return err("Request malloc", true);
  client->request.len = (ptrdiff_t)total_read;

  // buf can be reused now
  memcpy(client->request.data, buf, total_read);

  if (handle_request(client) < 0)
    return err("Handling request", true);
  print_client(client);

  // continue ahead with response

  char *res = "hello world.";
  write(client->fd, res, strlen(res));

  free_client(client);
  return 0;
}

void print_client(Client *client) {
  if (!client)
    return;

  // Even if the client has an ip4 address, the client_address is filled with
  // a ip6 mapped ip4 address
  // Therefore, no need for dealing with both here individually
  char ipstr[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)(client->address))->sin6_addr),
            ipstr, sizeof ipstr);

  printf("%s: %.*s", ipstr, (int)client->request.len, client->request.data);
}

void free_client(Client *client) {
  if (!client)
    return;

  // No need to free sockaddr_storage, is in server.c

  str_free(&(client->request));
};
