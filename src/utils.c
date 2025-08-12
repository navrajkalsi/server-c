#include "../include/utils.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void err_n_die(const char *msg, bool print_errno) {
  fputs(msg, stderr);
  if (print_errno)
    fprintf(stderr, "Error: {\n\tCode: %d\n\tMessage: %s\n}\n", errno,
            strerror(errno));
  exit(EXIT_FAILURE);
}
