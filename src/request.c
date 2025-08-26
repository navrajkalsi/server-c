#include "../include/request.h"
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// Array of filepaths to be served statically
const char *STATIC_FILES[] = {ICON_ICO, SERVER_HTML, SERVER_JS, ERROR_HTML};
const size_t STATIC_COUNT = sizeof STATIC_FILES / sizeof STATIC_FILES[0];

// Arrays of absolute paths of each file
// HAVE TO MAINTAIN BOTH arrays in the future
// found out macro concatention only works with string literals and not even
// char *s
const Str STATIC_PATHS[] = {
    STR(STATIC_PATH(ICON_ICO)), STR(STATIC_PATH(SERVER_HTML)),
    STR(STATIC_PATH(SERVER_JS)), STR(STATIC_PATH(ERROR_HTML))};

int handle_request(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // Cutting method
  // Head should just read GET
  // Tail everything after that
  Cut c = cut(client->request, ' ');
  if (!(c.head.data) || validate_method(&(c.head)) <= 0) {
    client->response_status = STR("405 Method Not Allowed");
    return err("Invalid method", false);
  }
  client->request_method = c.head;

  // Cutting path
  c = cut(c.tail, ' ');
  client->request_path =
      c.head; // assigning before verifying because i need to
              // print the request as is before erroring out or simplifying it
  print_request(client);

  if (!(client->request_path.data) ||
      validate_path(&(client->request_path), &(client->request_static)) != 0) {
    if (errno == ENOENT)
      client->response_status = STR("404 Not Found");
    else if (errno == EACCES)
      client->response_status = STR("403 Forbidden");
    return err("Invalid path", true);
  } else
    client->response_status = STR("200 OK");
  // The path exists and points to a valid file or dir which i can access
  // first I was using realpath :)

  // getting http version of the request
  // have to split with newline now
  client->http_ver = cut(c.tail, '\n').head;

  return 0;
}

int validate_method(Str *method) {
  if (!method || !(method->data))
    return null_ptr("Invalid method pointer");

  // Just checking for GET
  if (equals(method, &(STR("GET"))))
    return 1;

  return 0;
}

// Does depth checking, thanks to: skeeto, again
int validate_path(Str *path, bool *is_static) {
  if (!path || !(path->data))
    return null_ptr("Invalid path pointer");

  if (!(path->len) || (path->data)[0] != '/')
    return err("Invalid path", false);

  ptrdiff_t depth = 0;

  // Starting by moving ahead of the first '/'
  for (Cut c = {.tail = drophead(*path, 1)}; c.tail.len;) {
    c = cut(c.tail, '/');
    Str path_seg = c.head;

    if (equals(&path_seg, &(STR(".."))) && --depth < 0) {
      errno = EPERM;
      return err("Path above root", true);
    } else if (equals(&path_seg, &(STR(""))) || equals(&path_seg, &(STR("."))))
      continue; // not counting empty or current directory segs
    else
      depth++;
  }

  // now the path is checked to not go above the root
  // Checking if the file exists
  // To make a path like: '////////file' work
  // Shifting path.data so only one / remains in the beginning
  while (path->data[1] == '/') {
    path->data++;
    path->len--;
  }

  // shifting by -1 and adding null pointer to use stat()
  for (int i = 0; i < path->len - 1; i++)
    path->data[i] = path->data[i + 1];
  path->data[path->len - 1] = '\0';

  // now comparing against the static filepaths
  // this also sets the path to the final ABSOLUTE path of the file
  *is_static = check_static(path);

  // now the path is finals for static resources request and user requests

  // check if the path exists and if i can acces it
  return path_exists(path->data);
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

// returns -1 on error, 0 if not static, 1 if static
// ABSOLUTE path is set, this is the only place in the server where absolute
// paths are used
bool check_static(Str *path) {
  if (!path)
    return false;

  for (u_long i = 0; i < STATIC_COUNT; i++)
    if (!strcmp(path->data, STATIC_FILES[i])) {
      // Tried concatenating here, but did not work cause i need string literals
      *path = STATIC_PATHS[i];
      return true;
    }

  return false;
}
