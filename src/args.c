#include "../include/args.h"
#include "../include/utils.h"
#include <ctype.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Config parse_args(const int argc, char *const argv[]) {
  // Root dir, Acceptable incoming IP, Port, Debug
  Config server_config = {DEFAULT_ROOT_DIR, INADDR_LOOPBACK, DEFAULT_PORT, 0};

  int arg; // cannot be char, although the switch will compare it to char,
           // because getopt() can return -1 as well, therefore we will be
           // comparing the ASCII values of char literals

  // ':' is required to tell if the flag requires an argument after the flag in
  // cmd line
  unsigned int args_parsed = 0; // For debugging
  while ((arg = getopt(argc, argv, "adhp:r:")) != -1) {
    switch (arg) {
    case 'a':
      server_config.client_addr_t = INADDR_ANY;
      args_parsed++;
      break;
    case 'd':
      server_config.debug = 1;
      args_parsed++;
      break;
    case 'h':
      printf(
          "Usage: %s [OPTIONS] [ARGS...]\n"
          "Options:\n"
          "-a             Accept Incoming Connections from all IPs, defaults "
          "to Localhost only.\n"
          "-d             Debug Mode, prints every major function call.\n"
          "-h             Print this help message.\n"
          "-p <port>      Port to listen on.\n"
          "-r <directory> Directory to serve.\n",
          argv[0]);
      exit(EXIT_SUCCESS);
    case 'p':
      server_config.port = (uint16_t)atoi(
          optarg); // 'optarg' is a global variable set by getopt()
      // Have to convert it from ASCII string to integer
      // with atoi()
      args_parsed++;
      break;
    case 'r':
      if (!realpath(optarg, server_config.root_dir))
        err_n_die("Setting Root Directory");
      args_parsed++;
      break;
    case '?': // If an unknown flag or no argument is passed for an option
              // 'optopt' is set to the flag
              //
      if (optopt == 'p')
        fprintf(
            stderr,
            "Option '-p' requires passing a valid port number\nUse '-h' for "
            "usage.\n");
      else if (optopt == 'r')
        fprintf(
            stderr,
            "Option '-r' requries passing a valid directory path\nUse '-h' for "
            "usage.\n");
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option: '-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character used!\n");
      exit(EXIT_FAILURE);
    default:
      fprintf(stderr, "Unknown error occurred while parsing arguments\n");
      exit(EXIT_FAILURE);
    }
  }

  if (server_config.debug == 1) {
    printf("Debug Mode On.\n"
           "Parsed %d Argument(s).\n\n"
           "Root Directory set to: %s\n"
           "Port set to: %d\n",
           args_parsed, server_config.root_dir, server_config.port);
    if (server_config.client_addr_t == INADDR_ANY)
      puts("Server Accepting Incoming Connections from all IPs.\n");
    else
      puts("Server Accepting Incoming Connections from Localhost Only.\n");
  }

  return server_config;
}
