#pragma once

#include "client.h"

int handle_request(Client *client);

// Validates if the request method is GET, only GET supported for now
int validate_method(Str method);

int validate_path(Str path);

// null termianted path pointer
int path_exists(const char *path);

void print_request(Client *client);
