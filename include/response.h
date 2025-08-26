#pragma once

#include "client.h"

int handle_response(Client *client);

int write_response(Client *client);

int write_str(Client *client, Str *str);

// opens file/dir requested and reads it into client.response
int generate_response(Client *client);

// opens and reads the file into client.response
int read_dynamic_file(Client *client);

// Reads into the static response Str
// Essentially just reads SERVER_HTML file, NOT every static file
// All other FILES are read into the dynamic response body
int read_static_file(Client *client, const char *filepath);

int read_directory(Client *client);

void print_response(Client *client);

int find_delimiter(Client *client);
