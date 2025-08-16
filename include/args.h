#pragma once

#include "main.h"
#include "utils.h"
#include <netinet/in.h>
#include <stdint.h>

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
void print_usage(char *prg);

// Prints parsed args
void print_args(unsigned int args_parsed, const Config *cfg);

// Validates port from optarg, exits on error, points out to port
int validate_port(const char *port, uint16_t *out);

// Validates root_dir, exits on error, points out to root
int validate_root(const char *root_dir, Str *out);

// Validates if root_dir is actually a directory and not a 'file'
int is_dir(Str *root_dir);

// Free root_dir.data
void free_config(Config *cfg);
