#pragma once

#include <stdbool.h>
#include <stddef.h>

// Huge time saver string struct
typedef struct
{
  char *data;
  ptrdiff_t len;
} Str;

// Thanks to: skeeto on Reddit:)
typedef struct strNode
{
  struct strNode *next;
  Str *str;
} StrNode;

typedef struct
{
  StrNode *head;
  StrNode *tail;
} StrList;

typedef struct
{
  Str head;
  Str tail;
  bool found;
} Cut;

// Converts a null terminated string to a malloced Str
// The str is NOT MALLOCED, just the data
Str str_data_malloc(const char *in);

// The str and data BOTH ARE MALLOCED
Str *str_malloc(const char *in);

void str_data_free(Str *in);

void str_free(Str **in);

void str_print(const Str *in);

// strcmp like
bool equals(const Str *a, const Str *b);

// returns Str which points to starting of str but with take len, if possible
Str takehead(Str str, ptrdiff_t take);
// since Str args in both are copies, simply could change the str and return
// returns Str which points to str.len - drop, if possible
Str drophead(Str str, ptrdiff_t drop);

// cuts a string around the separator without copying str
// The head and tail are just pointers to the org str with different lengths and starting values
Cut cut(Str str, char sep);

// checks if str contains chars anywhere
// returns the index (if found), otherwise 0
ptrdiff_t contains(const Str *str, const char *chars);

// returns Str after combining two Strs
Str join(Str a, Str b);

// Always returns false
bool err(const char *msg, bool print_errno);

// Same as err just exits after printing
void err_n_die(const char *msg, bool print_errno);

bool setup_sig_handler(void);

void handle_shutdown(int sig);

void handle_sigpipe(int sig);

// Always sets errno to EFAULT & returns false
// to be returned it null ptrs are passed to a func
bool null_ptr(const char *msg);

void print_banner(void);

// Converts an int to a malloced, null terminated Str
// The user has to call free on the out string
// I am really proud of this function, as this is my first recursive function
// that I conceived in my brain and made to work
bool int_to_string(int i, Str *out);

// Prints if debug mode is set to on
// always returns true
bool print_debug(const char *msg);

void node_free(StrNode *node);

void list_free(StrList *list);

void list_append(StrList *list, StrNode *node);

StrNode *str_node_malloc(Str *str);

void str_node_free(StrNode **node);

void list_print(StrList *list);
