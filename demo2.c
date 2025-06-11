#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Macros
#define PORT 1419
#define BACKLOG 10
#define MAX_CLIENTS                                                            \
  100 // Could remove this in favour of a linked list of clients which can grow
      // dynamically
// Request related
#define READ_BUFFER_SIZE 2048
#define METHOD_SIZE 10
#define PATH_SIZE 1024

// Client struct, store information on a client: file descriptor (returned by
// accept function) and client_address (filled by accept())
struct client_info {
  int client_fd;
  struct sockaddr client_address;
};

// Main server socket file descriptor
int server_fd;
struct sockaddr_in server_address;

void err_n_die(const char operation[]) {
  printf("%s failed!\n", operation);
  printf("Error Code: %d\n", errno);
  printf("Error Message: %s\n", strerror(errno));
  exit(-1);
}

int parse_request(const char *request, char *method, char *path) {
  // Change safe limits here based on max method and path sizes
  // 1 less because of null pointer
  // Also change the number of returned args in case more args are required in
  // future
  printf("Received Request: \n\n%s", request);
  if (sscanf(request,
             "%9s\n"
             "%1023s\n",
             method, path) != 2)
    return -1;
  printf("Request Method: %s\n", method);
  printf("Request Path: %s\n", path);
  return 0;
}

// Takes in name of the file to write to the response buffer and sets size of
// the buffer, if the file does not exist errors and sets errno
// Pointer to reponse pointer is required to change the response in the main
// function
int generate_response(const char *path, char **response, unsigned int *size) {

  // Thanks Prof Kevin Forest!
  // Skipping the beginning '/' by shifting the pointer to the next char
  path += sizeof(char);
  printf("Path: %s\n", path);

  FILE *file = fopen(path, "r");

  if (file == NULL)
    return -1;

  fseek(file, 0, SEEK_END); // Seeking to the end of file
  long pos = ftell(file);   // Storing length of file
  rewind(file);             // Rewinding to start to copy the contents

  // Response buffer with null-terminator
  *response = (char *)malloc(pos + 1);

  // Number of bytes read, used as size of response
  *size = fread(*response, sizeof(char), pos, file);
  // Lesson learned
  (*response)[*size] = '\0';

  fclose(file);
  return 0;
}

int main(void) {
  // Variables
  // When it is time to grow this to multiple clients, all request and response
  // data will be client-specific (in client struct)
  // Reading requests
  char read_buffer[READ_BUFFER_SIZE];
  char request_method[METHOD_SIZE];
  char request_path[PATH_SIZE];

  // Responding
  char *response = NULL;
  unsigned int response_len = 0;

  // This returns a socket file descriptor as an int, which is like a two way
  // door. This is through which all communication takes place. It takes in
  // three params:
  // 1. Address/Protocol family
  // 2. Socket type (stream or datagram, mainly)
  // 3. Protocol family (0: OS chooses the appropriate one, TCP for stream
  // sockets & UDP for datagram sockets)
  server_fd = socket(AF_INET, SOCK_STREAM, 0);

  // Now the socket has to be binded to an IP & a port, the address should be of
  // any one of the interfaces on this machine. After binding all request to
  // this socket will be routed to that particular IP/port. The address can be:
  // INADDR_ANY: it is used to target all the avaliable interfaces on the
  // machine. Localhost: to target localhost, have to use the loopback address
  // (127.0.0.1) Any IP on the network, also have to convert it to binary form
  // :: is used to bind to all IPv6 addresses.
  // Note: Cannot just assign a string IP address to the server, have to convert
  // it to binary with inet_pton()
  // Also the port has to be  a short in network byte order(big endian), with
  // htons()
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  server_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&server_address,
           sizeof(server_address)) == -1)
    err_n_die("Binding");

  // Now we can start listening with the socket on the ip and port assigned
  if (listen(server_fd, BACKLOG) == -1)
    err_n_die("Listening");

  // Accepting connections, requires the server_fd and an empty 'struct
  // sockaddr' and its length. On connection, fills it with the address info of
  // the client
  // After accepting, all communication with the said client is done on the new
  // client_fd and server_fd still remains open listening for new conenctions.
  struct client_info new_client;
  struct sockaddr client_address = new_client.client_address;
  socklen_t add_len = sizeof(client_address);
  int client_fd = accept(server_fd, &client_address, &add_len);
  if (client_fd == -1)
    err_n_die("Accepting");
  new_client.client_fd = client_fd;

  // Reading and responding
  // Now we can read the request from the client and send any response.
  if (read(new_client.client_fd, read_buffer, READ_BUFFER_SIZE) == -1)
    err_n_die("Reading");

  if (parse_request(&read_buffer[0], &request_method[0], &request_path[0]) ==
      -1)
    err_n_die("Parsing Request");

  if (generate_response(&request_path[0], &response, &response_len) == -1)
    err_n_die("Generating Response");

  if (write(new_client.client_fd, response, response_len) == -1)
    err_n_die("Writing Response");

  if (close(new_client.client_fd) == -1)
    err_n_die("Closing Connection");

  return 0;
}
