#include "../include/args.h"
#include "../include/utils.h"
#include <bits/getopt_core.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Config parse_args(int argc, char *const argv[]) {
  // Root dir, Acceptable incoming IP, Port, Debug
  Config cfg = {{NULL, 0}, INADDR_LOOPBACK, DEFAULT_PORT, false};

  int arg; // cannot be char, although the switch will compare it to char,
           // because getopt() can return -1 as well, therefore we will be
           // comparing the ASCII values of char literals

  // ':' is required to tell if the flag requires an argument after the flag in
  // cmd line
  unsigned int args_parsed = 0; // For debugging
  while ((arg = getopt(argc, argv, "adhp:r:")) != -1) {
    switch (arg) {
    case 'a':
      cfg.client_addr_t = INADDR_ANY;
      args_parsed++;
      break;
    case 'd':
      cfg.debug = true;
      args_parsed++;
      break;
    case 'h':
      print_usage(argv[0]);
      exit(EXIT_SUCCESS);
    case 'p':
      cfg.port = validate_port(optarg);
      args_parsed++;
      break;
    case 'r':
      cfg.root_dir = validate_root(optarg);
      args_parsed++;
      break;
    case '?': // If an unknown flag or no argument is passed for an option
              // 'optopt' is set to the flag
      if (optopt == 'p')
        fputs("Option '-p' requires passing a valid port number\n"
              "Use '-h' for usage.\n",
              stderr);
      else if (optopt == 'r')
        fputs("Option '-r' requries passing a valid directory path\n"
              "Use '-h' for usage.\n",
              stderr);
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option: '-%c'.\n", optopt);
      else
        fputs("Unknown option character used!\n", stderr);
      exit(EXIT_FAILURE);
    default:
      fputs("Unknown error occurred while parsing arguments\n", stderr);
      exit(EXIT_FAILURE);
    }
  }

  // If -r not supplied, then using ./ as root of server
  if (!cfg.root_dir.data)
    cfg.root_dir = validate_root(DEFAULT_ROOT_DIR);

  print_args(args_parsed, &cfg);

  return cfg;
}

void print_usage(char *const prg) {
  printf("Usage: %s [OPTIONS] [ARGS...]\n"
         "Options:\n"
         "-a             Accept Incoming Connections from all IPs, defaults "
         "to Localhost only.\n"
         "-d             Debug Mode, prints every major function call.\n"
         "-h             Print this help message.\n"
         "-p <port>      Port to listen on.\n"
         "-r <directory> Directory to serve.\n",
         prg);
}

void print_args(unsigned int args_parsed, const Config *cfg) {
  printf("Parsed %u Argument(s).\n"
         "Root Directory set to: %s\n"
         "Port set to: %u\n"
         "Debug Mode set to: %s\n",
         args_parsed, cfg->root_dir.data, cfg->port, cfg->debug ? "On" : "Off");
  if (cfg->client_addr_t == INADDR_ANY)
    puts("Server Accepting Incoming Connections from all IPs.\n");
  else
    puts("Server Accepting Incoming Connections from Localhost Only.\n");
}

uint16_t validate_port(const char *port_arg) {
  if (!port_arg)
    err_n_die("Validating port failed.\nNull pointer passed.\n", false);
  // errno can be set to any non-zero value by a library function call
  // regardless of whether there was an error, so it needs to be cleared
  // in order to check the error set by strtol
  errno = 0;
  char *end;
  // 'optarg' is a global variable set by getopt()
  const long port = strtol(port_arg, &end, 10);
  if (*end != '\0') {
    err_n_die("Please enter a valid port number between 0 and 65535!\n", false);
  } else if (port <= 0 || port > 65535)
    err_n_die("Port number is out of range!\n", true);

  return (uint16_t)port;
}

Str validate_root(const char *root_dir) {
  if (!root_dir)
    err_n_die("Validating root_dir failed.\nNull pointer passed.\n", false);

  Str root_S = {NULL, 0};

  // By passing NULL, realpath allocates memory on its own
  // Owner has to free the memory allocated by realpath
  root_S.data = realpath(root_dir, NULL);

  if (!root_S.data)
    err_n_die("Validating root_dir failed.\nRealpath error.\n", true);

  root_S.len = (ptrdiff_t)strlen(root_S.data);

  return root_S;
}

void free_config(Config *cfg) {
  if (!cfg)
    err_n_die("Freeing config failed.\nNull pointer passed.\n", false);

  Str root_S = cfg->root_dir;
  if (root_S.data)
    free(root_S.data);

  root_S.data = NULL;
  root_S.len = 0;

  return;
}
