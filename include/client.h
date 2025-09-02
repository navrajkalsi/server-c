#pragma once

#include "server.h"
#include "utils.h"
#include <stddef.h>

// Client struct, store information on a client: file descriptor (returned by
// accept function) ,client_address (filled by accept()) which can be parsed to
// version 4 or 6 depending on usecase, address_len (also filled by accept()),
// pointer to read_buffer (to read request into), pointer to request_method
// (filled by parse_request()), pointer to request_path (also filled by
// parse_request())
typedef struct {
  struct sockaddr_storage *address;
  Str request;
  Str request_method;
  Str request_path;
  Str http_ver;
  Str dynamic_response_body;
  // Could not think of a better way to deal with this
  // Dynamic response is filled with what the user request directly, be it the
  // directory content or a file. Static response Str is filled with what code
  // the server needs to send in order to render the response(eg. _server.html,
  // in addition to the dir contents)
  Str static_response_body;
  Str response_status;
  Str content_type;
  Str content_length;         // to be used as content-length header
  Str connection;             // keep-alive or close
  Str date;                   // current date and time
  ptrdiff_t static_delimiter; // index of ~ in the SERVER_HTML in case a dir is
                              // requested
  int fd;
  socklen_t address_len;
  bool request_static;
} Client;

bool handle_client(Client *client);

// useful in debugging
void print_client(const Client *client);

// just checks and frees the response bodies, as they are the only malloced vars
void free_client(Client *client);
