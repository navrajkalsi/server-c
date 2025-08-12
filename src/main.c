#include "../include/args.h"

int main(int argc, char *argv[]) {

  Config config = parse_args(argc, argv);

  free_config(&config);

  return 0;
}
