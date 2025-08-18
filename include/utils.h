#pragma once

#include "main.h"
#include <stdbool.h>
#include <stddef.h>

// Huge time saver string struct
typedef struct {
  char *data;
  ptrdiff_t len;
} Str;

int str_init(Str *out);

void str_free(Str *in);

void err_n_die(const char *operation, bool print_errno);

int setup_sig_handler(void);

void handle_shutdown(int sig);
