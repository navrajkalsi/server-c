#include "../include/request.h"
#include <asm-generic/errno-base.h>
#include <errno.h>

int handle_request(Client *client) {
  if (!client) {
    errno = EFAULT;
    return -1;
  }

  return 0;
}
