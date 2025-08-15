#pragma once

#include "main.h"
#include "utils.h"

#ifdef OS_UNIX
#include <sys/socket.h>
#elifdef OS_WIN
#include <winsock2.h>
#else
#error Unable to set OS.
#endif

// Supported request methods enum
typedef enum { GET } Request_Method;

// Client struct, store information on a client: file descriptor (returned by
// accept function) ,client_address (filled by accept()) which can be parsed to
// version 4 or 6 depending on usecase, address_len (also filled by accept()),
// pointer to read_buffer (to read request into), pointer to request_method
// (filled by parse_request()), pointer to request_path (also filled by
// parse_request())
typedef struct {
  // struct sockaddr_storage client_address;
  Str *request;
  Str *request_path;
  Str *response;
  Str *reponse_status;
  Request_Method request_method;
  int client_fd;
  socklen_t address_len;
} Client;
