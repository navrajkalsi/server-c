#pragma once

// Args.c specific
#define DEFAULT_PORT "1419"
#define DEFAULT_ROOT_DIR "./"

// Server.c specific
#define BACKLOG 10

// Utils.c specific
extern bool RUNNING; // For shutdown handling
#define ERR_STR (Str){NULL, 0}
#define STR(str) (Str){str, sizeof(str) - 1}

// Client.c specific
#define BUF_MAX 8192
