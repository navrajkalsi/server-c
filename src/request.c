#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "client.h"
#include "main.h"
#include "request.h"
#include "utils.h"

// Array of filepaths to be served statically
const char *STATIC_FILES[] = {ICON_ICO, SERVER_HTML, SERVER_JS, ERROR_HTML};

// Arrays of absolute paths of each file
// HAVE TO MAINTAIN BOTH arrays in the future
// found out macro concatention only works with string literals and not even
// char *s
const Str STATIC_PATHS[] = {
    STR(STATIC_PATH(ICON_ICO)), STR(STATIC_PATH(SERVER_HTML)),
    STR(STATIC_PATH(SERVER_JS)), STR(STATIC_PATH(ERROR_HTML))};

bool handle_request(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // Cutting method
  // Head should just read GET
  // Tail everything after that
  Cut c = cut(client->request, ' ');
  if (!(client->request_method = c.head)
           .data) { // Request is not a valid http request
    client->response_status =
        ASSIGN_IF_NULL(client->response_status, "400 Bad Request");
    return err("Cutting method", false); // Cannot do much else in this func now
  }
  if (!validate_method(&client->request_method)) { // Method is invalid
    client->response_status =
        ASSIGN_IF_NULL(client->response_status, "405 Method Not Allowed");
    err("Invalid method", false);
  }
  print_debug("Parsed method");

  // Cutting path & params
  c = cut(c.tail, ' ');
  Cut path_cut = cut(c.head, '?'); // separating path & params

  if (!(client->request_path = path_cut.head)
           .data) { // Request is not a valid http request
    client->response_status =
        ASSIGN_IF_NULL(client->response_status, "400 Bad Request");
    return err("Cutting path", false);
  }
  print_request(client);

  if (!validate_path(
          &client->request_path,
          &client->request_static)) { // Path is not valid for some reason
    if (errno == EINVAL) // path is invalid, does not start with / or failed to
                         // decode or simplify
      client->response_status =
          ASSIGN_IF_NULL(client->response_status, "400 Bad Request");
    else if (errno == EACCES)
      client->response_status =
          ASSIGN_IF_NULL(client->response_status, "403 Forbidden");
    else if (errno == ENOENT) // path does not exist
      client->response_status =
          ASSIGN_IF_NULL(client->response_status, "404 Not Found");
    err("Invalid path", true);
  }
  // The path exists and points to a valid file or dir which i can access
  // first I was using realpath :)
  print_debug("Parsed request path");

  // Checking params
  if (path_cut.found) {
    if (!parse_params(client, &path_cut.tail))
      err("Parsing params", false);
    print_debug("Parsed params");
  }

  // getting http version of the request
  // have to split with carraige return now
  c = cut(c.tail, '\n'); // not assigning to http_ver, if the string is invalid
                         // using the default http_ver of the server
  if (c.head.len && c.head.data[c.head.len - 1] == '\r')
    c.head.len--; // this way malformed request that only use '\n' are also
                  // supported

  // returns 400 for HTTP/0.9
  if (!c.head.len || !c.head.data || !validate_http(&c.head)) {
    client->response_status =
        ASSIGN_IF_NULL(client->response_status, "400 Bad Request");
    return err("Cutting & validating HTTP version", false);
  }

  if (equals(&c.head, &STR("HTTP/2.0")) || equals(&c.head, &STR("HTTP/3.0")))
    client->response_status = ASSIGN_IF_NULL(client->response_status,
                                             "505 HTTP Version Not Supported");
  else {
    client->http_ver = c.head; // only assigning if version is valid and
                               // supported, otherwise v1.1 is used
    client->response_status = ASSIGN_IF_NULL(client->response_status, "200 OK");
  }
  print_debug("Parsed HTTP version");

  if (!set_connection(&client->connection, &c.tail))
    if (equals(&client->http_ver, &STR("HTTP/1.0"))) // setting default for 1.0
      client->connection = STR("close");

  return true;
}

bool validate_method(const Str *method) {
  if (!method || !(method->data))
    return null_ptr("Invalid method pointer");

  // Just checking for GET
  return equals(method, &(STR("GET"))) &&
         print_debug("Request method validated");
}

// Does depth checking, thanks to: skeeto, again
bool validate_path(Str *path, bool *is_static) {
  if (!path || !(path->data))
    return null_ptr("Invalid path pointer");

  if (!(path->len) || (path->data)[0] != '/') {
    errno = EINVAL;
    return err("Invalid path", true);
  }

  ptrdiff_t depth = 0;

  // Starting by moving ahead of the first '/'
  for (Cut c = {.tail = drophead(*path, 1)}; c.tail.len;) {
    c = cut(c.tail, '/');
    Str path_seg = c.head;

    if (equals(&path_seg, &(STR(".."))) && --depth < 0) {
      errno = EACCES;
      return err("Path above root", true);
    } else if (equals(&path_seg, &(STR(""))) || equals(&path_seg, &(STR("."))))
      continue; // not counting empty or current directory segs
    else
      depth++;
  }

  // now the path is checked to not go above the root

  if (!simplify_path(path)) {
    if (!errno)
      errno = EINVAL;
    return err("Simplifying URL", false);
  }

  // decoding ascii chars
  if (!decode_path(path)) {
    if (!errno)
      errno = EINVAL;
    return err("Decoding URL", false);
  }

  // now comparing against the static filepaths
  // this also sets the path to the final ABSOLUTE path of the file
  *is_static = check_static(path);

  // now the path is final for static resources request and user requests
  // check if the path exists and if i can acces it
  return path_exists(path->data) && print_debug("Validated request path");
}

bool simplify_path(Str *path) {
  if (!path)
    return null_ptr("Invalid path pointer");

  // To make a path like: '////////file' work
  // Shifting path.data so only one / remains in the beginning
  while (path->len > 1 && path->data[1] == '/') {
    path->data++;
    path->len--;
  }

  // shifting by -1 and adding null pointer to use stat()
  for (int i = 0; i < path->len - 1; i++)
    path->data[i] = path->data[i + 1];
  path->data[--path->len] = '\0';

  // if the path is null after shifting and len was one (now 0 after removing
  // /), that means current directory is requested and request path was '/'
  if (*(path->data) == '\0' && path->len == 0)
    *path = STR("./");

  return true;
}

bool decode_path(Str *path) {
  if (!path)
    return null_ptr("Invalid path pointer");

  ptrdiff_t pos = 0;

  // ascii chars contains only two hex digits after %
  while (pos < path->len) {
    if (pos < path->len - 2 && path->data[pos] == '%') { // decoding %
      char *end = NULL;
      const char encoded[3] = {path->data[pos + 1], path->data[pos + 2], '\0'};
      long decoded = strtol(encoded, &end, 16);

      if (!decoded && *end != '\0')
        return err("Converting from hex str to int", false);

      // decoded will now contain the decimal form of the ascii char
      path->data[pos++] = (char)decoded;

      // moving chars over
      memmove(path->data + pos, path->data + pos + 2,
              path->len - pos - 1); // also moving \0
      path->len -= 2;
    } else if (path->data[pos] == '+') // + to space
      path->data[pos++] = ' ';
    else
      ++pos;
  }

  return true;
}

bool parse_params(Client *client, const Str *params) {
  if (!client || !params)
    null_ptr("Invalid client or params pointer");

  // checking individual params, in future could change for cut implementation
  ptrdiff_t index = contains(params, "show_dir");

  if (index == -1)
    return true;
  // moving ahead of =
  index += sizeof "show_dir";

  if (params->len - index < 5 && memcmp(params->data + index, "true", 4) == 0)
    client->show_dir = true;
  else
    client->show_dir = false;

  return true;
}

bool validate_http(const Str *http_ver) {
  if (!http_ver)
    return null_ptr("Invalid HTTP pointer");

  if (equals(http_ver, &STR("HTTP/1.0")) ||
      equals(http_ver, &STR("HTTP/1.1")) ||
      equals(http_ver, &STR("HTTP/2.0")) || equals(http_ver, &STR("HTTP/3.0")))
    return print_debug("Validated HTTP version");

  return false;
}

bool set_connection(Str *connection, Str *request) {
  if (!request || !request->len || !request->data)
    return null_ptr("Invalid request pointer");

  Cut c = {0};
  c.tail = *request;
  size_t len = sizeof "Connection:" - 1;

  while (c.tail.len && c.tail.data) {
    c = cut(c.tail, '\n');
    if (strncasecmp(c.head.data, "Connection:", len) == 0)
      break;
  }

  if (!c.tail.len || !c.tail.data || !c.head.len || !c.head.data) // not found
    return false;

  // c.head contains the connection str
  // shifting to point to the header value
  size_t shift = len;
  for (; isspace(c.head.data[shift]); shift++)
    ;
  c.head.data += shift;
  c.head.len -= (ptrdiff_t)shift;

  if (strncasecmp(c.head.data, "keep-alive", sizeof("keep-alive") - 1) == 0)
    *connection = STR("keep-alive");
  else if (strncasecmp(c.head.data, "close", sizeof("close") - 1) == 0)
    *connection = STR("close");
  else
    return false; // now the connection has to be set depending on the http
                  // version

  return print_debug("Set connection header");
}

bool path_exists(const char *path) {
  struct stat s;
  return stat(path, &s) == 0;
}

void print_request(const Client *client) {
  if (!client)
    return (void)null_ptr("Invalid client pointer");

  // Even if the client has an ip4 address, the client_address is filled with
  // a ip6 mapped ip4 address
  // Therefore, no need for dealing with both here individually
  char ipstr[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&client->address)->sin6_addr),
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
    return null_ptr("Invalid path pointer");

  for (u_long i = 0; i < LEN; i++)
    if (!strcmp(path->data, STATIC_FILES[i])) {
      // Tried concatenating here, but did not work cause i need string
      // literals
      *path = STATIC_PATHS[i];
      return print_debug("Static request detected");
    }

  return false;
}
