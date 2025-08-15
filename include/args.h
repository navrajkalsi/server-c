#pragma once

#include "main.h"
#include "utils.h"
#include <stdint.h>

#ifdef OS_UNIX
#include <netinet/in.h>
#elifdef OS_WIN
#include <winsock2.h>
#else
#error Unable to set OS.
#endif

typedef struct {
  Str root_dir;
  in_addr_t client_addr_t; // Incoming requests from localhost or other IPs also
  uint16_t port;
  bool debug;
} Config;

// Parses args from the command line, if any
// Errors and exits if the root_dir passed does not exist
Config parse_args(int argc, char *argv[]);

// Prints -h help
void print_usage(char *const prg);

// Prints parsed args
void print_args(unsigned int args_parsed, const Config *cfg);

// Validates port from optarg, exits on error, returns port on success
uint16_t validate_port(const char *port);

// Validates root_dir, exits on error, returns the Str struct
Str validate_root(const char *root_dir);

// Free root_dir.data
void free_config(Config *cfg);
