#include "../include/args.h"
#include "../include/server.h"

bool RUNNING = true;

int main(int argc, char *argv[]) {
  int server_fd;

  Config config = parse_args(argc, argv);

  server_fd = setup_server(&config);

  free_config(&config); // No need for this now

  if (server_fd == -1) {
    err_n_die("Setting server up failed", true);
  }

  if (start_server(server_fd) == -1)
    err_n_die("Server failed", true);

  return 0;
}
