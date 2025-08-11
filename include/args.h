#pragma once

#include "main.h"
#include <netinet/in.h>

typedef struct server_config {
  char root_dir[PATH_SIZE];
  in_addr_t client_addr_t; // Incoming requests from localhost or other IPs also
  uint16_t port;
  unsigned int debug : 1; // Bitfield, 0 or 1 possible only
} Config;

// Parses args from the command line, if any
// Errors and exits if the root_dir passed does not exist
Config parse_args(const int argc, char *const argv[]);
