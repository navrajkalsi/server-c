#pragma once

#include "server.h"
#include "utils.h"

// Client struct, store information on a client: file descriptor (returned by
// accept function) ,client_address (filled by accept()) which can be parsed to
// version 4 or 6 depending on usecase, address_len (also filled by accept()),
// pointer to read_buffer (to read request into), pointer to request_method
// (filled by parse_request()), pointer to request_path (also filled by
// parse_request())
typedef struct {
  struct sockaddr_storage *address;
  Str *request;
  Str *request_path;
  Str *response;
  Str *reponse_status;
  RequestMethod request_method;
  int fd;
  socklen_t address_len;
} Client;

int client_init(Client *out);

int handle_client(Client *client);

void print_client(Client *client);

void free_client(Client *client);
