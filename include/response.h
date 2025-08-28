#pragma once

#include "client.h"

bool handle_response(Client *client);

bool write_response(Client *client);

bool write_headers(Client *client);

bool write_response_body(Client *client);

bool write_error(Client *client);

bool write_str(Client *client, const Str *str);

// opens file/dir requested and reads it into client.response
bool generate_response(Client *client);

// opens and reads the file into client.response
bool read_dynamic_file(Client *client);

// Reads into the static response Str
// Essentially just reads SERVER_HTML file, NOT every static file
// All other FILES are read into the dynamic response body
bool read_static_file(Client *client, const char *filepath);

bool read_directory(Client *client);

void print_response(Str *response_array[], int array_len);

bool find_delimiter(Client *client);

// if path is NULL, client.path is used
bool set_content_type(Client *client, const char *path);

// used to set content length in case a directory is requested
// As it uses the util: int_to_string() and that does not seem to work with
// non-text based filetypes
bool set_content_length(Client *client);
