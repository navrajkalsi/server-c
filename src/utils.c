#include "../include/utils.h"
#include "../include/main.h"

#include <asm-generic/errno-base.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int str_init(Str *out) {
  if (out && (out = (Str *)malloc(sizeof(Str))))
    return 0;

  errno = EFAULT;
  return -1;
}

void str_free(Str *in) {
  if (in && in->data) {
    free(in->data);
    in->len = 0;
  }
  return;
}

void err_n_die(const char *msg, bool print_errno) {
  fputs(msg, stderr);
  if (print_errno && errno)
    fprintf(stderr, "Error: {\n\tCode: %d\n\tMessage: %s\n}\n", errno,
            strerror(errno));
  exit(EXIT_FAILURE);
}

int setup_sig_handler(void) {
  // Handling shutdown
  struct sigaction sa_shutdown;
  sa_shutdown.sa_handler = handle_shutdown;
  sigemptyset(&sa_shutdown.sa_mask);
  sa_shutdown.sa_flags = 0; // No flags required for shutting down

  // SIGINT (signal interput) is sent when Ctrl+C is pressed
  // SIGTERM (signal terminate) is sent when the process is killed from like
  // terminal with kill command
  if (sigaction(SIGINT, &sa_shutdown, NULL) == -1 ||
      sigaction(SIGTERM, &sa_shutdown, NULL) == -1)
    return -1;

  return 0;
}

void handle_shutdown(int sig) {
  (void)sig;
  RUNNING = false;
  return;
}
