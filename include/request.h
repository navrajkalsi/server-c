#pragma once

#include "client.h"

// Also used in response.c
typedef enum { ICO, HTML, JS, ERR, LEN } STATIC_INDICES;
extern const char *STATIC_FILES[];
extern const Str STATIC_PATHS[];

bool handle_request(Client *client);

// Validates if the request method is GET, only GET supported for now
bool validate_method(const Str *method);

bool validate_path(Str *path, bool *is_static);

bool validate_http(const Str *http_ver);

// finds "connection" header in request, returns false if not found
// otherwise sets the connection to version default
bool set_connection(Str *connection, Str *request);

// null termianted path pointer
bool path_exists(const char *path);

void print_request(const Client *client);

// compares the path to list of static files
// Also set the path to the ABSOLUTE path of the static file
bool check_static(Str *path);
