#pragma once

#include "client.h"
#include <stddef.h>

// Also used in response.c
extern const char *STATIC_FILES[];
extern const Str STATIC_PATHS[];
extern const size_t STATIC_COUNT;

int handle_request(Client *client);

// Validates if the request method is GET, only GET supported for now
int validate_method(Str *method);

int validate_path(Str *path, bool *is_static);

// null termianted path pointer
int path_exists(const char *path);

void print_request(Client *client);

// compares the path to list of static files
// Also set the path to the ABSOLUTE path of the static file
bool check_static(Str *path);
