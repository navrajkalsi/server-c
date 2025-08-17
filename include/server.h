#pragma once

#include "args.h"
#include "main.h"
#include "utils.h"
#include <sys/socket.h>

// Supported request methods enum
typedef enum { GET } RequestMethod;

// Calls all the functions, setting up the server and returns the file
// descriptor for the server socket, or -1 on error
int setup_server(Config *cfg);

// The actual loop accepting connections
int start_server(int server_fd);
