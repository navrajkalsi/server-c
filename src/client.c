#include "../include/client.h"
#include "../include/request.h"
#include "../include/response.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool handle_client(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // buffer to be used for data storage for unknown length types
  // client.request then points to this buffer
  // no need for void, only gonna use chars
  char buf[BUF_MAX], *buf_ptr = buf;
  size_t total_read = 0;
  long read_status = 0;
  char *end_ptr;

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

  if (read_status != -1 && end_ptr) {
    // Advancing the ptr by 4 chars to get past the request end
    // Then comparing with buf_ptr to see if they are same
    // If same that means there is no body after headers and the total_read is
    // the correct length, else change total_read to the length of only the
    // request headers
    end_ptr = &(end_ptr[TRAILER.len]);
    if (end_ptr != buf_ptr)
      // Discard if there is any body in the request
      // Only supporting GET requests for now
      total_read = (size_t)(end_ptr - buf);
  } else {
    errno = errno ? errno : EMSGSIZE;
    err("Reading request", true);
  }

  // At this point total_read is the correct len of data in buf
  // request can only be used in this scope!!
  client->request.data = buf;
  client->request.len = (ptrdiff_t)total_read;

  client->response_status =
      errno == EMSGSIZE ? STR("431 Request Header Fields Too Large") : ERR_STR;

  // Handle request sets the required response codes
  // In case no response code is set, means the function errored and
  // handle_response will send 500 code
  if (!handle_request(client))
    err("Handling request", true);

  if (errno == EMSGSIZE)
    client->response_status = STR("431 Request Header Fields Too Large");

  // If the response_status is not set at this point, then that means either
  // read() or handle_request() errored
  if (!handle_response(client))
    return err("Handling response", true);

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

void free_client(Client *client) {
  if (!client)
    return;

  if (client->dynamic_response_body.len)
    str_free(&client->dynamic_response_body);

  // both response bodies would be malloced at some point if they exist
  if (client->static_response_body.len)
    str_free(&client->static_response_body);

  if (client->response_mime.len)
    str_free(&client->response_mime);

  if (client->response_body_len.len)
    str_free(&client->response_body_len);

  return;
}
