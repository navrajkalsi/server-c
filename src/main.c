#include <stdbool.h>

#include "args.h"
#include "main.h"
#include "server.h"
#include "threads.h"
#include "utils.h"

bool RUNNING = true;
SSL_CTX *ssl_context = NULL;
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

  free_config_data(&config);

  return 0;
}
