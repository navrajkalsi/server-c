#pragma once

#include "main.h"
#include "utils.h"

#ifdef OS_UNIX
#include <netinet/in.h>
#elifdef OS_WIN
#include <winsock2.h>
#else
#error Unable to set OS.
#endif

// Thanks windows for making me do this...
// Have to write a custom implementation of getopt(), thanks to windows
// Struct for each opt housing all data about that option
typedef struct {
  char opt;
  char *name;
  bool argRequired;
} Opt;

// Return struct from get_opt()
typedef struct {
  char opt;
  char *arg;
} OptOut;

// get_opt(), like getopt()
// Errors in case of no arg, or providing arg where no arg is required
// Fills OptInput with the data
void get_opt(int argc, char *argv[], Opt *opts, const unsigned int opts_len,
             OptOut *out);

// To verify if a opt is valid and exists in opts
void is_opt_valid(char *argv[], const char *opt, Opt *opts,
                  const unsigned int opts_len, Opt *out);
