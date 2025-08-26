#include "../include/client.h"
#include "../include/request.h"
#include "../include/response.h"
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int handle_client(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // Temporary buffer to be used for data storage for unknown length types
  // no need for void, only gonna use chars
  char buf[BUF_MAX], *buf_ptr = buf;
  size_t total_read = 0;
  long read_status = 0;
  const char *request_end = "\r\n\r\n";
  char *end_ptr;

  while ((read_status = read(client->fd, buf_ptr, BUF_MAX - total_read - 1)) >
         0) {
    total_read += (size_t)read_status;
    buf_ptr = &buf[total_read];
    buf[total_read] = '\0';

    if ((end_ptr = strstr(buf, request_end)))
      // No need to read more
      break;
  }

  if (read_status != -1 && end_ptr) {
    // Advancing the ptr by 4 chars to get past the request end
    // Then comparing with buf_ptr to see if they are same
    // If same that means there is no body after headers and the total_read is
    // the correct length else change total_read to the length of only the
    // request headers
    end_ptr = &(end_ptr[strlen(request_end)]);
    if (end_ptr != buf_ptr)
      // Discard if there is any body in the request
      // Only supporting GET requests for now
      total_read = (size_t)(end_ptr - buf);
  } else {
    errno = errno ? errno : EMSGSIZE;
    return err("Reading request", true);
  }

  // At this point total_read is the correct len of str
  client->request = STR(buf);
  // buf can be reused now

  // Handle request sets the required response codes
  if (handle_request(client) < 0)
    return err("Handling request", true);

  if (handle_response(client) < 0)
    return err("Handling response", true);

  return 0;
}

void print_client(Client *client) {
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

void free_client(Client *client) {
  if (!client)
    return;

  if (client->dynamic_response_body.len)
    str_free(&client->dynamic_response_body);

  // both response bodies would be malloced at some point if they exist
  if (client->static_response_body.len)
    str_free(&client->static_response_body);

  return;
}
