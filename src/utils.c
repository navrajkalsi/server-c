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

void str_free(Str *in) {
  if (in && in->data && in->len) {
    free(in->data);
    in->data = NULL;
    in->len = 0;
  }
  return;
}

bool equals(const Str *a, const Str *b) {
  return a->len == b->len && !memcmp(a->data, b->data, (size_t)(a->len));
}

// returns 0 len str in case of error
Str takehead(Str str, ptrdiff_t take) {
  if (!str.data || str.len < 0)
    return ERR_STR;

  str.len = take > str.len ? str.len : take;
  return str;
}

Str drophead(Str str, ptrdiff_t drop) {
  if (!str.data || str.len < 0 || drop > str.len)
    return ERR_STR;

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

  return ret;
}

Str join(Str a, Str b) {
  if (!a.data || !b.data || a.len < 0 || b.len < 0)
    return ERR_STR;

  char joined[a.len + b.len + 1];

  memcpy(joined, a.data, (size_t)a.len);
  memcpy(joined + a.len, b.data, (size_t)b.len);
  joined[a.len + b.len] = '\0';

  return STR(joined);
}

bool err(const char *msg, bool print_errno) {
  // fputs(msg, stderr);
  // if (print_errno && errno)
  //   fprintf(stderr, "Error: {\n\tCode: %d\n\tMessage: %s\n}\n", errno,
  //           strerror(errno));
  if (print_errno && errno)
    perror(msg);
  else
    fprintf(stderr, "%s\n", msg);
  return false;
}

void err_n_die(const char *msg, bool print_errno) {
  (void)err(msg, print_errno);
  exit(EXIT_FAILURE);
}

bool setup_sig_handler(void) {
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
    return false;

  return true;
}

void handle_shutdown(int sig) {
  (void)sig;
  RUNNING = false;
  return;
}

bool null_ptr(const char *msg) {
  errno = EFAULT;
  return err(msg, true);
}

void print_banner(void) {
  puts("\n\n");
  puts("  ███████╗███████╗██████╗ ██╗   ██╗███████╗██████╗        ██████╗");
  puts("  ██╔════╝██╔════╝██╔══██╗██║   ██║██╔════╝██╔══██╗      ██╔════╝");
  puts("  ███████╗█████╗  ██████╔╝██║   ██║█████╗  ██████╔╝█████╗██║     ");
  puts("  ╚════██║██╔══╝  ██╔══██╗╚██╗ ██╔╝██╔══╝  ██╔══██╗╚════╝██║     ");
  puts("  ███████║███████╗██║  ██║ ╚████╔╝ ███████╗██║  ██║      ╚██████╗");
  puts("  ╚══════╝╚══════╝╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚═╝  ╚═╝       ╚═════╝");
  puts("\n\n");
}

bool int_to_string(int i, Str *out) {
  static ptrdiff_t pos =
      0; // the chars have to be written from the beginning, therefore this
         // would serve as the index where the char would go

  // set to ERR_STR before passing it in
  // base case, when last single int is divided by 10, 0 is returned
  if (i == 0) {
    if (!(out->data = (char *)malloc((size_t)out->len)))
      return err("Malloc string data", true);
    return true;
  }

  out->len++;
  if (!int_to_string(i / 10, out))
    return err("Converting int to string recursive", false);
  *(out->data + pos++) = (char)((i % 10) + '0');

  // Setting pos to 0 to reuse later
  if (pos == out->len)
    pos = 0;
  return true;
}
