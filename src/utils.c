#include "../include/utils.h"
#include "../include/main.h"

#include <asm-generic/errno-base.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
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

// returns -1 len in case of error
Str takehead(Str str, ptrdiff_t take) {
  if (take > str.len) {
    str.len = -1;
    return str;
  }

  str.len = take;
  return str;
}

Str drophead(Str str, ptrdiff_t drop) {
  if (drop > str.len)
    return (Str){};

  str.data += drop;
  str.len -= drop;
  return str;
}

Cut cut(Str str, char sep) {
  ptrdiff_t pos = 0;

  while (pos < str.len && str.data[pos] != sep)
    pos++;

  Cut ret = {};
  ret.found = pos < str.len;
  ret.head = takehead(str, pos);
  ret.tail = drophead(str, pos + ret.found);

  if (!(ret.head.data) || !(ret.tail.data))
    return (Cut){};

  return ret;
}

int err(const char *msg, bool print_errno) {
  // fputs(msg, stderr);
  // if (print_errno && errno)
  //   fprintf(stderr, "Error: {\n\tCode: %d\n\tMessage: %s\n}\n", errno,
  //           strerror(errno));
  if (print_errno && errno)
    perror(msg);
  else
    fputs(msg, stderr);
  return -1;
}

void err_n_die(const char *msg, bool print_errno) {
  (void)err(msg, print_errno);
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

int null_ptr(const char *msg) {
  errno = EFAULT;
  return err(msg, true);
}
