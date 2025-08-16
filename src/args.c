#include "../include/args.h"
#include "../include/utils.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

Config parse_args(int argc, char *argv[]) {
  // Root dir, Acceptable incoming IP, Port, Debug
  Config cfg = {{NULL, 0}, INADDR_LOOPBACK, DEFAULT_PORT, false};

  int arg; // cannot be char, although the switch will compare it to char,
           // because getopt() can return -1 as well, therefore we will be
           // comparing the ASCII values of char literals

  // ':' is required to tell if the flag requires an argument after the flag
  // in cmd line
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
      if (validate_port(optarg, &(cfg.port)) < 0) {
        free_config(&cfg);
        err_n_die("Setting port failed.\n", true);
      }
      args_parsed++;
      break;
    case 'r':
      if (validate_root(optarg, &(cfg.root_dir)) < 0) {
        free_config(&cfg);
        err_n_die("Setting root directory failed.\n", true);
      }
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
    if (validate_root(DEFAULT_ROOT_DIR, &(cfg.root_dir)) < 0) {
      free_config(&cfg);
      err_n_die("Setting root directory failed.\n", true);
    }

  print_args(args_parsed, &cfg);

  return cfg;
}

void print_usage(char *prg) {
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

int validate_port(const char *port_arg, uint16_t *out) {
  if (!port_arg || !out) {
    errno = EFAULT;
    return -1;
  }

  char *end;
  // 'optarg' is a global variable set by getopt()
  const long port = strtol(port_arg, &end, 10);
  if (*end != '\0') {
    fputs("Please enter a valid port number between 0 and 65535!\n", stderr);
    errno = EINVAL;
    return -1;
  } else if (port < 0 || port > 65535) {
    fputs("Port number is out of range!\n", stderr);
    errno = EDOM;
    return -1;
  }

  *out = (uint16_t)port;
  return 0;
}

int validate_root(const char *root_dir, Str *out) {
  if (!root_dir || !out) {
    errno = EFAULT;
    return -1;
  }

  // By passing NULL, realpath allocates memory on its own
  // Owner has to free the memory allocated by realpath
  out->data = realpath(root_dir, NULL);

  if (!out->data) // realpath sets errno
    return -1;

  out->len = (ptrdiff_t)strlen(out->data);

  if (!is_dir(out))
    return 0;
  else
    return -1;
}

int is_dir(Str *root_dir) {
  if (!root_dir || !root_dir->data) {
    errno = EFAULT;
    return -1;
  }

  // Metadata for the root
  struct stat root_stat;
  if (stat(root_dir->data, &root_stat) == -1) // stat sets errno
    return -1;

  if (S_ISDIR(root_stat.st_mode))
    return 0;
  if (S_ISREG(root_stat.st_mode)) {
    fputs("It seems the argument for root directory points to a file and not a "
          "directory.\n",
          stderr);
    errno = EINVAL;
    return -1;
  } else {
    fputs("Either the root directory does not exist, is a file, or you do not "
          "have the permissions to access it.\n",
          stderr);
    errno = EINVAL;
    return -1;
  }
}

void free_config(Config *cfg) {
  if (!cfg)
    err_n_die("Freeing config failed.\nNull pointer passed.\n", false);

  Str *root_S = &(cfg->root_dir);

  if (root_S->data)
    free(root_S->data);

  root_S->data = NULL;
  root_S->len = 0;

  return;
}
