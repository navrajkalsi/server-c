#include "../include/client.h"
#include "../include/request.h"
#include "../include/response.h"
#include <arpa/inet.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

static ClientNode *head = NULL;
static ClientNode *tail = NULL;

bool handle_client(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  do {
    // freeing previous members and not the client struct itself
    free_client_members(client);

    // Using poll to know when reading(POLLIN) is possible
    struct pollfd poll_fd = {.fd = client->fd, .events = POLLIN};
    int result =
        poll(&poll_fd, 1,
             10000); // 10s timeout for any incoming data selects only 1 fd
    if (!result ||
        (poll_fd.revents &
         (POLLERR | POLLHUP))) // no data to read, can return to handle_thread
      break;

    if (result < 0)
      return err("Poll", true);

    // buffer to be used for data storage for unknown length types
    // client.request then points to this buffer
    // no need for void, only gonna use chars
    char buf[BUF_MAX], *buf_ptr = buf;
    size_t total_read = 0;
    long read_status = 0;
    char *end_ptr = NULL;

    // Only headers are considered, cause I currently support GET only, anything
    // after TRAILER is disregarded
    while ((read_status = read(client->fd, buf_ptr, BUF_MAX - total_read - 1)) >
           0) {
      total_read += (size_t)read_status;
      buf_ptr = &buf[total_read];
      buf[total_read] = '\0';

      if ((end_ptr = strstr(buf, TRAILER.data)))
        // No need to read more
        break;
    }

    // if there is nothing to read, client closes
    if (buf == buf_ptr &&
        !total_read) // I always receive an empty request in the beginning,
                     // as the browser connects and immediately disconnects
      break;         // this is not a bug, just how TCP works

    if (read_status != -1 &&
        end_ptr) { // everything is alright, got the headers
      // Advancing the ptr by 4 chars to get past the request end
      // Then comparing with buf_ptr to see if they are same
      // If same that means there is no body after headers and the total_read is
      // the correct length, else change total_read to the length of only the
      // request headers
      end_ptr = &(end_ptr[TRAILER.len]);
      if (end_ptr != buf_ptr)
        // Discard if there is any body in the request
        // Only supporting GET requests for now
        // so don't need any body
        total_read = (size_t)(end_ptr - buf);
    } else if (read_status != -1 && !end_ptr) { // could not find end of headers
      // first looking if i got the first request line, if not the request is
      // just not a valid request probably (bad request) looking for a linebreak
      // "\r\n" for request line
      client->response_status =
          !strstr(buf, LINEBREAK.data)
              ? STR("400 Bad Request") // no need to parse request now
              : STR("431 Request Header Fields Too Large");
    } else {
      err("Reading request", true);
      // Reading next request now
      // continue;
    }

    // At this point total_read is the correct len of data in buf
    // request can only be used in this scope!!
    client->request.data = buf;
    client->request.len = (ptrdiff_t)total_read;

    // Handle request sets the required response codes
    // Only parsing (handle_request) if the request line is present
    if (!equals(&client->response_status, &STR("400 Bad Request")) &&
        !handle_request(client))
      err("Handling request", true);

    if (!handle_response(client))
      err("Handling response", true);

  } while (equals(&client->connection, &STR("keep-alive")));

  return true;
}

void print_client(const Client *client) {
  if (!client)
    return;

  if (client->request.len)
    printf("Request: %.*s\n", (int)client->request.len, client->request.data);
  if (client->request_method.len)
    printf("Request Method: %.*s\n", (int)client->request_method.len,
           client->request_method.data);
  if (client->request_path.len)
    printf("Request Path: %.*s\n", (int)client->request_path.len,
           client->request_path.data);
  if (client->http_ver.len)
    printf("Request HTTP version: %.*s\n", (int)client->http_ver.len,
           client->http_ver.data);
  if (client->response_status.len)
    printf("Response Status: %.*s\n", (int)client->response_status.len,
           client->response_status.data);
  printf("Request Static: %s\n",
         client->request_static ? "Static" : "User request");
}

void free_client(Client **client) {
  if (!client)
    return;

  free_client_members(*client);
  free(*client);
}

void free_client_members(Client *client) {
  if (!client)
    return;

  if (client->dynamic_response_body.len)
    str_free(&client->dynamic_response_body);

  // both response bodies would be malloced at some point if they exist
  if (client->static_response_body.len)
    str_free(&client->static_response_body);

  if (client->content_type.len)
    str_free(&client->content_type);

  if (client->content_length.len)
    str_free(&client->content_length);

  if (client->date.len)
    str_free(&client->date);
}

void enqueue_client(Client *client) {
  if (!client)
    return;

  ClientNode *new_node;
  if (!(new_node = (ClientNode *)malloc(sizeof(ClientNode)))) {
    err("Malloc client node", true);
    return;
  }

  new_node->client = client;
  new_node->next = NULL;

  if (!tail)
    head = new_node;
  else
    tail->next = new_node;

  tail = new_node;
}

Client *dequeue_client(void) {
  if (!head)
    return NULL;

  Client *result = head->client;
  ClientNode *temp = head;
  head = head->next;

  if (!head)
    tail = NULL;

  free(temp);
  return result;
}

Client *client_init(void) {
  Client *client = (Client *)malloc(sizeof *client);
  if (!client)
    return NULL;

  { // Str
    client->request = client->request_method = client->request_path =
        client->dynamic_response_body = client->static_response_body =
            client->response_status = client->content_type =
                client->content_length = client->date = ERR_STR;
  }

  client->http_ver = STR("HTTP/1.1");
  client->connection = STR("keep-alive");
  // client->response_status = STR("500 Internal Server Error");
  client->fd = -1;
  client->address = NULL;
  client->address_len = sizeof(struct sockaddr_storage);
  client->request_static = false;

  return client;
}

void print_list(void) {
  ClientNode *current = head;

  if (!current)
    return;

  do
    printf("Fd: %d\n", current->client->fd);
  while ((current = current->next));
}
