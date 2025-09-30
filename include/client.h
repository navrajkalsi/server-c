#pragma once

#include "utils.h"
#include <openssl/crypto.h>
#include <stddef.h>
#include <sys/socket.h>

// Client struct, store information on a client: file descriptor (returned by
// accept function) ,client_address (filled by accept()) which can be parsed to
// version 4 or 6 depending on usecase, address_len (also filled by accept()),
// pointer to read_buffer (to read request into), pointer to request_method
// (filled by parse_request()), pointer to request_path (also filled by
// parse_request())
typedef struct {
  struct sockaddr_storage address; // address info of the client
  Str request;
  Str request_method;
  Str request_path;
  Str http_ver;
  Str dynamic_response_body;
  // Could not think of a better way to deal with this
  // Dynamic response is filled with what the user request directly, be it the
  // directory content or a file. Static response Str is filled with what code
  // the server needs to send in order to render the response(eg. _server.html,
  // in addition to the dir contents)
  Str static_response_body;
  Str response_status;
  Str content_type;
  Str content_length;         // to be used as content-length header
  Str connection;             // keep-alive or close
  Str date;                   // current date and time
  SSL *ssl;                   // ssl object for client, in case of https
  ptrdiff_t static_delimiter; // index of ~ in the SERVER_HTML in case a dir is
                              // requested
  int fd;
  socklen_t address_len;
  bool request_static;
  bool show_dir; // from url param, defualts to previewing dir, rather than
                 // serving index.html
} Client;

// Linked list node for clients, for threading
typedef struct client_node {
  Client *client;
  struct client_node *next;
} ClientNode;

bool handle_client(Client *client);

// useful in debugging
void print_client(const Client *client);

// frees the members and the struct itself
void free_client(Client **client);

// just frees the client struct members
void free_client_members(Client *client);

// adds client struct to list of clients read to be handled by threads
void enqueue_client(Client *client);

// removes a client from the list when the response is done or an error occurs
Client *dequeue_client(void);

// initializes the client with malloc and fills it with default values of fields
// user calls free
Client *client_init(void);

// prints the clients linked list
void print_list(void);
