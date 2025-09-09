#include <stdbool.h>

#include "../include/args.h"
#include "../include/main.h"
#include "../include/server.h"
#include "../include/threads.h"

bool RUNNING = true;
Config config;

int main(int argc, char *argv[]) {
  print_banner();

  config = parse_args(argc, argv);

  int server_fd;
  if (!setup_server(&config, &server_fd))
    err_n_die("Setting server up failed", true);

  if (!create_threads())
    err_n_die("Creating threads", true);

  if (!start_server(server_fd))
    err_n_die("Server failed", true);

  return 0;
}
