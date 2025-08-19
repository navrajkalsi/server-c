#include "../include/request.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int handle_request(Client *client) {
  if (!client)
    return err("Invalid client pointer", true);

  // Cutting method
  // Head should just read GET
  // Tail everything after that
  Cut c = cut(client->request, ' ');
  if (!(c.head.data) || validate_method(&c.head) != 0)
    return -1;

  // Cutting path
  c = cut(c.tail, ' ');

  printf("head: %.*s\n", (int)(c.head.len), c.head.data);
  printf("tail: %.*s\n", (int)(c.tail.len), c.tail.data);
  return 0;
}

int validate_method(Str *method) {
  if (!method || !(method->data)) {
    errno = EFAULT;
    return -1;
  }

  // Just checking for GET
  if (memcmp(method->data, "GET", strlen("GET")))
    return 1;

  return 0;
}
