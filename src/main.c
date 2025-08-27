#include "../include/args.h"
#include "../include/server.h"

bool RUNNING = true;

int main(int argc, char *argv[]) {
  print_banner();

  Config config = parse_args(argc, argv);

  int server_fd;
  if (!setup_server(&config, &server_fd))
    err_n_die("Setting server up failed", true);

  if (!start_server(server_fd))
    err_n_die("Server failed", true);

  return 0;
}
