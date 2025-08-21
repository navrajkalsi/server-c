#pragma once

#include "main.h"
#include <stdbool.h>
#include <stddef.h>

// Huge time saver string struct
typedef struct {
  char *data;
  ptrdiff_t len;
} Str;

// Thanks to: skeeto on Reddit:)
typedef struct {
  Str head;
  Str tail;
  bool found;
} Cut;

int str_init(Str *out);

void str_free(Str *in);

// strcmp like
bool equals(Str a, Str b);

// returns Str which points to starting of str but with take len, if possible
Str takehead(Str str, ptrdiff_t take);
// since Str args in both are copies, simply could change the str and return
// returns Str which points to str.len - drop, if possible
Str drophead(Str str, ptrdiff_t drop);

// cuts a string around the separator without copying str
// The head and tail are just pointers to the org str with different lengths and
// starting values
Cut cut(Str str, char sep);

// Always returns -1
int err(const char *msg, bool print_errno);

// Same as err just exits after printing
void err_n_die(const char *msg, bool print_errno);

int setup_sig_handler(void);

void handle_shutdown(int sig);

// Always sets errno to EFAULT & returns -1
// to be returned it null ptrs are passed to a func
int null_ptr(const char *msg);
