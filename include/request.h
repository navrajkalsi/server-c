#pragma once

#include "client.h"

// Also used in response.c
typedef enum { ICO, HTML, JS, ERR, STATIC_LEN } STATIC_INDICES;
extern const char *STATIC_FILES[STATIC_LEN];
extern const Str STATIC_PATHS[STATIC_LEN];

bool handle_request(Client *client);

// Validates if the request method is GET, only GET supported for now
bool validate_method(const Str *method);

bool validate_path(Str *path, bool *is_static);

bool validate_http(const Str *http_ver);

// removes excessive '/'s and checks for current dir request
bool simplify_path(Str *path);

// decodes an url by converting hex digits to ascii chars
// does not support utf-8 yet
bool decode_path(Str *path);

bool parse_params(Client *client, const Str *params);

// finds "connection" header in request, returns false if not found
// otherwise sets the connection to version default
bool set_connection(Str *connection, Str *request);

// null termianted path pointer
bool path_exists(const char *path);

void print_request(const Client *client);

// compares the path to list of static files
// Also set the path to the ABSOLUTE path of the static file
bool check_static(Str *path);
