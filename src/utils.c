#include "../include/utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void err_n_die(const char *operation) {
  fprintf(stderr, "%s failed!\nError Code: %d\nError Message: %s\n\n",
          operation, errno, strerror(errno));
  exit(EXIT_FAILURE);
}
