#pragma once

// Args.c specific
#define DEFAULT_PORT "1419"
#define DEFAULT_ROOT_DIR "./"

// Server.c specific
#define BACKLOG 10

// Utils.c specific
extern bool RUNNING; // For shutdown handling
#define ERR_STR (Str){NULL, 0}
#define STR(str)                                                               \
  (Str) { str, (long)(sizeof(str) - 1) }

// Client.c specific
#define BUF_MAX 8192

// Request.c specific
#ifndef STATIC_DIR
#define STATIC_DIR "/usr/local/share/server-c/static"
#endif
// Helps me concat without using strcat, static dir with a static file
// Macros are amazing
#define STATIC_PATH(file) STATIC_DIR "/" file
// #define SERVER_HTML "_server.html"
#define SERVER_HTML "_server.html"
#define SERVER_JS "_server.js"
#define ERROR_HTML "_error.html"
#define ICON_ICO "favicon.ico"

// Response.c specific
#define TRAILER STR("\r\n\r\n")
#define SPACE STR(" ")
#define LINEBREAK STR("\r\n")
#define HTTP_DELIMITER "~"
// To convert numeric to string
#define TO_STRING_HELPER(x) #x
#define TO_STRING(x) TO_STRING_HELPER(x)
