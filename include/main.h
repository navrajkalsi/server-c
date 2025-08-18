#pragma once

// Args.c specific
#define DEFAULT_PORT "1419"
#define DEFAULT_ROOT_DIR "./"

// Server.c specific
#define BACKLOG 10

// Utils.c specific
extern bool RUNNING; // For shutdown handling

// Client.c specific
#define BUF_MAX 8192
