#pragma once

#include "main.h"
#include <stdbool.h>
#include <stddef.h>

// Huge time saver string struct
typedef struct {
  char *data;
  ptrdiff_t len;
} Str;

void err_n_die(const char *operation, bool print_errno);

int setup_sig_handler(void);

void handle_shutdown(int sig);
