#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "args.h"
#include "main.h"

Config parse_args(int argc, char *argv[]) {
  // Root dir, Acceptable incoming IP, Port, Debug
  Config cfg = {.root_dir = {NULL, 0},
                .port = DEFAULT_PORT,
                .accept_all = false,
                .debug = false};

  int arg; // cannot be char, although the switch will compare it to char,
           // because getopt() can return -1 as well, therefore we will be
           // comparing the ASCII values of char literals

  // ':' is required to tell if the flag requires an argument after the flag
  // in cmd line
  unsigned int args_parsed = 0; // For debugging
  while ((arg = getopt(argc, argv, "adhp:r:")) != -1) {
    switch (arg) {
    case 'a':
      cfg.accept_all = true;
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
      if (!validate_port(optarg, &(cfg.port)))
        err_n_die("Invalid port number", true);
      args_parsed++;
      break;
    case 'r':
      if (!validate_root(optarg))
        err_n_die("Invalid root directory", true);
      cfg.root_dir = STR(optarg);
      args_parsed++;
      break;
    case '?': // If an unknown flag or no argument is passed for an option
              // 'optopt' is set to the flag
      if (optopt == 'p')
        arg_error('p', "requires a valid port number");
      else if (optopt == 'r')
        arg_error('r', "requires a valid directory path");
      else if (isprint(optopt))
        arg_error((char)optopt, "unknown option");
      else
        fputs("Unknown option character used!\n", stderr);
      exit(EXIT_FAILURE);
    default:
      fputs("Unknown error occurred while parsing arguments\n", stderr);
      exit(EXIT_FAILURE);
    }
  }

  // If -r not supplied, then using ./ as root of server
  if (!cfg.root_dir.data) {
    if (!validate_root(DEFAULT_ROOT_DIR))
      err_n_die("Setting root directory failed.\n", true);
    cfg.root_dir = STR(DEFAULT_ROOT_DIR);
  }

  print_args(args_parsed, &cfg);

  return cfg;
}

void print_usage(char *prg) {
  printf("\nUsage: %s [OPTIONS] [ARGS...]\n"
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
  if (args_parsed)
    printf("Parsed %u Argument(s).\n", args_parsed);

  printf("Root Directory set to: %s\n"
         "Port set to: %s\n"
         "Debug Mode set to: %s\n",
         cfg->root_dir.data, cfg->port, cfg->debug ? "On" : "Off");

  cfg->accept_all
      ? puts("Server Accepting Incoming Connections from all IPs.\n")
      : puts("Server Accepting Incoming Connections from Localhost Only.\n");
}

bool validate_port(char *port_arg, char **out) {
  if (!port_arg || !out)
    return null_ptr("Invalid port pointer");

  char *end;
  // 'optarg' is a global variable set by getopt()
  const long port = strtol(port_arg, &end, 10);
  if (*end != '\0') {
    errno = EINVAL; // not a valid number
    return false;
  }
  if (port < 0 || port > 65535) {
    errno = ERANGE; // out of range
    return false;
  }

  *out = port_arg;
  return true;
}

bool validate_root(const char *root_dir) {
  if (!root_dir)
    return null_ptr("Invalid root pointer");

  // No need to check the path, if it points to a dir or if it exists and
  // permissions Chdir does all that, and makes request handling much simpler
  // later
  return chdir(root_dir) == 0;
}

int is_dir(const Str *root_dir) {
  if (!root_dir || !root_dir->data)
    return null_ptr("Invalid root pointer");

  // Metadata for the root
  struct stat root_stat;
  if (stat(root_dir->data, &root_stat) == -1) // stat sets errno
    return -1;

  if (S_ISDIR(root_stat.st_mode))
    return 0;

  errno = ENOTDIR;
  return -1;
}

void arg_error(char opt, const char *msg) {
  fprintf(stderr, "Option '-%c' %s\nUse -h for usage.\n", opt, msg);
}
