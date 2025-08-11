#pragma once

#include <sys/socket.h>

// Client struct, store information on a client: file descriptor (returned by
// accept function) ,client_address (filled by accept()) which can be parsed to
// version 4 or 6 depending on usecase, address_len (also filled by accept()),
// pointer to read_buffer (to read request into), pointer to request_method
// (filled by parse_request()), pointer to request_path (also filled by
// parse_request())
struct client_info {
  struct sockaddr_storage client_address;
  int client_fd;
  socklen_t address_len;
  char read_buffer[READ_BUFFER_SIZE];
  char request_method[METHOD_SIZE];
  char request_path[PATH_SIZE];
  char *response;
  size_t response_len;
  char response_status[STATUS_SIZE];
}
