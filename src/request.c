#include "../include/request.h"
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int handle_request(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // Cutting method
  // Head should just read GET
  // Tail everything after that
  Cut c = cut(client->request, ' ');
  if (!(c.head.data) || validate_method(c.head) != 0)
    return err("Invalid method", false);
  client->request_method = c.head;

  // Cutting path
  c = cut(c.tail, ' ');
  client->request_path = c.head; // assigning before verifying because i need to
                                 // print the request before erroring out
  print_request(client);
  if (!(c.head.data) || validate_path(c.head) != 0)
    return err("Invalid Path", true);
  // The path exists and points to a valid file or dir
  // first I was using realpath :)

  return 0;
}

int validate_method(Str method) {
  if (!(method.data))
    return null_ptr("Invalid method pointer");

  // Just checking for GET
  if (!equals(method, STR("GET")))
    return 1;

  return 0;
}

// Does depth checking, thanks to: skeeto, again
int validate_path(Str path) {
  if (!(path.data))
    return null_ptr("Invalid path pointer");

  if (!(path.len) || (path.data)[0] != '/')
    return err("Invalid path", false);

  ptrdiff_t depth = 0;

  // Starting by moving ahead of the first '/'
  for (Cut c = {.tail = drophead(path, 1)}; c.tail.len;) {
    c = cut(c.tail, '/');
    Str path_seg = c.head;
    if (equals(path_seg, STR("..")) && --depth < 0) {
      errno = EPERM;
      return err("Path above root", true);
    } else if (equals(path_seg, STR("")) || equals(path_seg, STR(".")))
      continue; // not counting empty or current directory segs
    else
      depth++;
  }

  // now the path is checked to not go above the root
  // Checking if the file exists
  // Shifting path.data by -1 and adding null terminator to use stat()
  for (int i = 0; i < path.len - 1; i++)
    (path.data)[i] = (path.data)[i + 1];
  (path.data)[path.len - 1] = '\0';

  // check if the path exists
  return path_exists(path.data);
}

int path_exists(const char *path) {
  struct stat s;
  return stat(path, &s);
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

  // printf("%s: %.*s", ipstr, (int)client->request.len,
  // client->request.data);
  printf("\n(%s) %.*s %.*s\n", ipstr, (int)client->request_method.len,
         client->request_method.data, (int)client->request_path.len,
         client->request_path.data);
}
