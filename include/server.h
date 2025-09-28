#pragma once

#include "args.h"
#include <openssl/crypto.h>

// Supported request methods enum
// typedef enum { GET } RequestMethod; // For future use

// Calls all funcitons for setting up SSL
// returns the ssl context object
SSL_CTX *setup_ssl();

// Calls all the functions, setting up the server and sets the file
// descriptor for the server socket
// Also sets SSL
bool setup_server(Config *cfg, int *server_fd);

// The actual loop accepting connections
bool start_server(const int server_fd);
