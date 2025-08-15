#include "../include/get_opt.h"
#include "../include/args.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void get_opt(int argc, char *argv[], Opt *opts, const unsigned int opts_len,
             OptOut *out) {
  if (!argv || opts_len == 0 || !opts || !out)
    err_n_die("Retrieving options failed.\n", false);

  // Current opt to parse
  static int current_opt = 1;

  if (current_opt >= argc)
    return;

  // If opt starts with single '-' and the functions was not called by itself to
  // find the arg
  char *current_argv = argv[current_opt];
  if (*current_argv == '-') {
    if (current_argv[1] != '-') {  // only single char used
      if (current_argv[2] != '\0') // cannot join multiple flags
        err_n_die(
            "Please do not join flags.\nTry again with individual flags, or "
            "did you forget another '-'?\n",
            false);

      Opt valid_opt = {'\0', NULL, false};
      is_opt_valid(argv, &current_argv[1], opts, opts_len, &valid_opt);
      out->opt = valid_opt.opt;
      if (!valid_opt.argRequired)
        return;
      else {
        if (current_opt == argc || !(current_argv = argv[++current_opt]) ||
            *current_argv == '-') {
          // No argument detected for when required
          fprintf(stderr, "Option: '-%c' requires an argument.\n",
                  valid_opt.opt);
          print_usage(*argv);
          exit(EXIT_FAILURE);
        }

        // Copying string to dest
        if (!(out->arg = strdup(current_argv)))
          err_n_die("Copying Argument Failed.\n", true);
        return;
      }
    }
  }
}

void is_opt_valid(char *argv[], const char *opt, Opt *opts,
                  unsigned int opts_len, Opt *out) {
  if (opts_len == 0 || !opts || !out)
    err_n_die("Retrieving options failed.\n", false);

  // Going in reverse
  while (opts_len--) {
    Opt current_opt = opts[opts_len];
    if (strcmp(opt, &(current_opt.opt)) == 0 ||
        strcmp(opt, current_opt.name) == 0) {
      *out = opts[opts_len];
      return;
    }
  }

  print_usage(argv[0]);
  err_n_die("option not found.\n", false);
}
