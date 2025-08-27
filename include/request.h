#pragma once

#include "client.h"
#include <stddef.h>

// Also used in response.c
typedef enum { ICO, HTML, JS, ERR, LEN } STATIC_INDICES;
extern const char *STATIC_FILES[];
extern const Str STATIC_PATHS[];

bool handle_request(Client *client);

// Validates if the request method is GET, only GET supported for now
bool validate_method(Str *method);

bool validate_path(Str *path, bool *is_static);

// null termianted path pointer
bool path_exists(const char *path);

void print_request(Client *client);

// compares the path to list of static files
// Also set the path to the ABSOLUTE path of the static file
bool check_static(Str *path);
