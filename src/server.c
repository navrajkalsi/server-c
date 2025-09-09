#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../include/args.h"
#include "../include/client.h"
#include "../include/main.h"
#include "../include/threads.h"
#include "../include/utils.h"

bool setup_server(Config *cfg, int *server_fd) {
  if (!cfg)
    return null_ptr("Invalid config pointer");

  struct addrinfo hints, *out, *current;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6; // Dual stack, will also support IPv4
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  // If getaddrinfo() errors and does not change errno, then have to use
  // gai_strerror()
  // If errno changes, main() prints the error
  int getaddr_status = 0;
  errno = 0;
  // ai_flags=PASSIVE & domain=NULL is required for a socket to be binded
  if ((getaddr_status = getaddrinfo(cfg->accept_all ? "::" : "::1", cfg->port,
                                    &hints, &out)) < 0) {
    if (!errno)
      fputs(gai_strerror(getaddr_status), stderr);
    else
      err("Getting host info", true);
    return false;
  }

  current = out;

  do {
    // Creating a socket for the appropriate ip version
    // This returns a socket file descriptor as an int, which is like a two way
    // door. This is through which all communication takes place. It takes in
    // three params:
    // 1. Address/Protocol family
    // 2. Socket type (stream or datagram, mainly)
    // 3. Protocol family (0: OS chooses the appropriate one, TCP for stream
    // sockets & UDP for datagram sockets)
    if ((*server_fd = socket(current->ai_family, current->ai_socktype,
                             current->ai_protocol)) < 0)
      continue;

    // Have to setsocketopt to allow dual-stack setup supporting both IPv4 & v6
    if (setsockopt(*server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &(int){0},
                   sizeof(int)) < 0) {
      close(*server_fd);
      *server_fd = -1;
      continue;
    }

    if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1},
                   sizeof(int)) < 0) {
      close(*server_fd);
      *server_fd = -1;
      continue;
    }

    // Adding timeouts for read and write
    struct timeval time = {.tv_sec = 5, .tv_usec = 0};
    if (setsockopt(*server_fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof time) <
        0) {
      close(*server_fd);
      *server_fd = -1;
      continue;
    }

    if (setsockopt(*server_fd, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof time) <
        0) {
      close(*server_fd);
      *server_fd = -1;
      continue;
    }

    if (bind(*server_fd, current->ai_addr, current->ai_addrlen) < 0) {
      close(*server_fd);
      *server_fd = -1;
      continue;
    }

    // If a valid socket is binded to then break, else set server_fd to -1

    break;

    // Change position of break, in case need to see avaliable options
    if (current->ai_family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)(current->ai_addr);
      char addr_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(addr->sin_addr), addr_str, sizeof addr_str);
      puts("IP4");
      puts(addr_str);
    } else {
      struct sockaddr_in6 *addr = (struct sockaddr_in6 *)(current->ai_addr);
      char addr_str[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &(addr->sin6_addr), addr_str, sizeof addr_str);
      puts("IP6");
      puts(addr_str);
    }
  } while ((current = current->ai_next));

  freeaddrinfo(out);

  if (*server_fd == -1) {
    errno = EADDRNOTAVAIL;
    return err("Getting server file descriptor", true);
  }
  print_debug("Server file descriptor acquired");

  if (listen(*server_fd, BACKLOG) < 0)
    return err("Listening", true);

  printf("Server Listening on port: %s\n\n", cfg->port);
  return print_debug("Server setup");
}

bool start_server(const int server_fd) {
  if (server_fd < 0)
    return null_ptr("Invalid server descriptor"); // null erroring for now

  RUNNING = true;
  setup_sig_handler();

  // In the loop, a function call can error in two ways, if SIGTERM or SIGINT
  // is received || the function itself errors
  // In former, errno would be EINTR
  while (RUNNING) {
    Client *client = client_init();
    if (!client) {
      err("Initialize client", true);
      break;
    }

    client->address = &(struct sockaddr_storage){0};

    // sockaddr_storage is better to store addresses than sockaddr, if ip v is
    // not known beforehand
    // Though it is not necessary here, as all ips will be mapped to ip6
    if ((client->fd = accept(server_fd, (struct sockaddr *)client->address,
                             &(client->address_len))) < 0) {
      free_client(&client);
      if (errno == EINTR && !RUNNING)
        break; // shutdown

      if (errno == ECONNABORTED) {
        err("Connection aborted",
            true); // connection aborted, maybe client closed connection
        continue;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK)
        // incase the socket closes
        continue;

      err("Accepting connection", true);
      break;
    }

    print_debug("Accepted a new connection");

    pthread_mutex_lock(&mutex);
    enqueue_client(client);
    pthread_cond_signal(&condition_var); // now a thread is signalled and that
                                         // thread aquires the lock
    pthread_mutex_unlock(&mutex);
  }

  if (close(server_fd) < 0)
    return err("Closing server", true);

  print_debug("Server File Descriptor closed");

  // If RUNNING is still true, then a function call errored out
  // If a function errored due to interrupt signal, then the signal handler
  // will set RUNNING to false and then we can shutdown, otherwise this was an
  // actual error and return -1 If interrupted the errno at this point would
  // be EINTR
  if (RUNNING) {
    RUNNING = false;
    cleanup_pool();
    return err("Server terminated", true);
  }

  puts("\nShutting Down...\n");
  cleanup_pool();

  return true;
}
