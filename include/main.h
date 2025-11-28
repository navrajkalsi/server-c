#pragma once

#include <openssl/crypto.h>
#include <pthread.h>
#include <stdbool.h>

// Args.c specific
#define VERSION "2.3"
#define DEFAULT_PORT "1419"
#define DEFAULT_ROOT_DIR "./"

// Server.c specific
#define BACKLOG 256
#define THREAD_POOL_SIZE 25
// SSL files, can be changed during compilation
#ifndef DOMAIN_CERT
#define DOMAIN_CERT "/etc/ssl/domain/domain.cert"
#endif
#ifndef PRIVATE_KEY
#define PRIVATE_KEY "/etc/ssl/domain/private.key"
#endif

// Utils.c specific
#define ERR_STR (Str){NULL, 0}
#define STR(str)                                                                                   \
  (Str) { str, (long)(sizeof(str) - 1) }

// Client.c specific
#define BUF_MAX 8192

// Request.c specific
#ifndef STATIC_DIR
#define STATIC_DIR "/usr/local/share/server-c/static"
#endif
// Helps me concat without using strcat, static dir with a static file Macros are amazing
#define STATIC_PATH(file) STATIC_DIR "/" file
// #define SERVER_HTML "_server.html"
#define SERVER_HTML "_server.html"
#define SERVER_JS "_server.js"
#define ERROR_HTML "_error.html"
#define ICON_ICO "favicon.ico"
// only to assign the string literal to str.data if str.data is null
#define ASSIGN_IF_NULL(str, literal) !str.data ? STR(literal) : str

// Response.c specific
#define SERVER "Server-C/" VERSION " (Unix)"
#define TRAILER STR("\r\n\r\n")
#define SPACE STR(" ")
#define LINEBREAK STR("\r\n")
#define HTTP_DELIMITER "~"
// To convert numeric to string
#define TO_STRING_HELPER(x) #x
#define TO_STRING(x) TO_STRING_HELPER(x)
// Date len is the length of a date for http header plus a null terminator
#define DATE_LEN 30

extern bool RUNNING;         // for shutdown handling and thread cleaning
extern SSL_CTX *ssl_context; // this is thread safe, no mutex required
extern pthread_mutex_t mutex;
extern pthread_cond_t condition_var;
