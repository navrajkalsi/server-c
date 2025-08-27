#pragma once

#include "main.h"
#include "utils.h"
#include <netinet/in.h>
#include <stdint.h>

typedef struct {
  Str root_dir;
  char *port;
  bool accept_all; // accept requests from localhost only or from all IPs
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
bool validate_port(char *port, char **out);

// Validates root_dir, exits on error, by calling chdir which handles
// permission, errno. Much better than realpath()
bool validate_root(const char *root_dir);

// Prints error for individual args
void arg_error(char opt, const char *msg);
