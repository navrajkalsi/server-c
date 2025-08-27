#pragma once

#include "args.h"
#include "main.h"
#include "utils.h"
#include <sys/socket.h>

// Supported request methods enum
// typedef enum { GET } RequestMethod; // For future use

// Calls all the functions, setting up the server and sets the file
// descriptor for the server socket
bool setup_server(Config *cfg, int *server_fd);

// The actual loop accepting connections
bool start_server(const int server_fd);
