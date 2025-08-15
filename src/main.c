#include "../include/args.h"
#include "../include/get_opt.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

  Opt opts[] = {{'a', NULL, true}, {'b', NULL, false}};

  OptOut out = {'\0', NULL};
  get_opt(argc, argv, opts, 1, &out);

  printf("flag: %c\n", out.opt);
  printf("arg: %s\n", out.arg);

  free(out.arg);
  // Config config = parse_args(argc, argv);

  // free_config(&config);

  return 0;
}
