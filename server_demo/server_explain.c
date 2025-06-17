#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <dirent.h>
#include <errno.h>
#include <magic.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Need to compile with -lmagic flag to use magic.h for get_mime_type()

// Macros
#define PORT 1419
#define BACKLOG 10
// Request related
#define READ_BUFFER_SIZE 4096
#define METHOD_SIZE 10
#define PATH_SIZE 4096
// Response related
#define STATUS_SIZE 32

// Variable to determine running status of server
volatile sig_atomic_t running = 1;

// Client struct, store information on a client: file descriptor (returned by
// accept function) ,client_address (filled by accept()) which can be parsed to
// version 4 or 6 depending on usecase, address_len (also filled by accept()),
// pointer to read_buffer (to read request into), pointer to request_method
// (filled by parse_request()), pointer to request_path (also filled by
// parse_request())
struct client_info {
  int client_fd;
  struct sockaddr_storage client_address;
  socklen_t address_len;
  char read_buffer[READ_BUFFER_SIZE];
  char request_method[METHOD_SIZE];
  char request_path[PATH_SIZE];
  char *response;
  unsigned int response_len;
  char response_status[STATUS_SIZE];
};

// Main server socket file descriptor
int server_fd;
struct sockaddr_in server_address;

// Root dir of server
char root_dir[PATH_SIZE];

void err_n_die(const char operation[]) {
  printf("%s failed!\n", operation);
  printf("Error Code: %d\n", errno);
  printf("Error Message: %s\n", strerror(errno));
  exit(EXIT_FAILURE);
}

void shutdown_handler(int s) {
  (void)s;
  running = 0;
}

void sigchild_handler(int s) {
  (void)s;

  // waitpid might change the errno
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

// Sets global variable 'root_dir' to the working directory
int set_root_dir(void) {
  if (!realpath("./", root_dir))
    return -1;
  return 0;
}

// Handles requesting of any static files like favicon
// Returns 1 if the path has to be dealt with statically and not to be used with
// realpath() in parse_request()
int check_static_request(struct client_info *client) {
  if (!client)
    return -1;

  if (strcmp(client->request_path, "/favicon.ico") == 0)
    return 1;
  return 0;
}

// Parses a request, extracting the 'request_method' & 'request_path'.
// Request path is converted to absolute path and checked for traversal
int parse_request(struct client_info *client) {
  // Change safe limits here based on max method and path sizes
  // 1 less because of null pointer
  // Also change the number of returned args in case more args are required in
  // future
  printf("Received Request: \n\n%s", client->read_buffer);
  if (sscanf(client->read_buffer, "%9s %4095s", client->request_method,
             client->request_path) != 2)
    return -1;

  int is_path_static = check_static_request(client);

  if (is_path_static == 0) { // Absolute path is to be used
    // Thanks Prof Kevin Forest!
    // Skipping the beginning '/' by shifting the pointer to the next char
    // Getting actual absolute path of the target
    if (!realpath(&(client->request_path)[1], client->request_path)) {
      // If the errno is set to 2, that means the requested directory/file does
      // not exist Then the server serves the '404.html file instead'
      if (errno == 2) {
        snprintf(client->response_status, STATUS_SIZE,
                 "404 Not Found"); // Setting status code
        if (!realpath("404.html",
                      client->request_path)) // If the file does not exist,
                                             // request path is set to the
                                             // absolute path of 404.html file
          return -1;
      } else
        return -1;
    }

    // Comparing the starting chars of the 'root_dir' and 'request_path' to
    // prevent any path traversal
    if (strncmp(root_dir, client->request_path, strlen(root_dir)) != 0) {
      errno = EPERM;
      return -1;
    }
  } else if (is_path_static == -1)
    return -1;
  else // File is static and needs to be served from the server path. Removing
       // the starting '/'
    strncpy(client->request_path, &(client->request_path)[1], PATH_SIZE);

  return 0;
}

// Returns the mime type of a file
const char *get_mime_type(const char *filepath) {
  magic_t magic = magic_open(MAGIC_MIME_TYPE);
  if (magic == NULL)
    return NULL;
  if (magic_load(magic, NULL) != 0) {
    magic_close(magic);
    return NULL;
  }

  const char *mime = magic_file(magic, filepath);
  magic_close(magic);

  return mime;
}

// Generates an HTTP response header
int generate_header(char **header, char *status, const char *content_type,
                    unsigned int content_length, unsigned int *header_size) {
  if (status[0] == '\0')
    snprintf(status, STATUS_SIZE, "200 OK");

  const char *header_template = "HTTP/1.1 %s\r\n Content-Type: %s\r\n "
                                "Content-Length: %u\r\n Connection: "
                                "close\r\n\r\n";

  int final_len = snprintf(NULL, 0, header_template, status, content_type,
                           content_length); // calculating just the final length
  *header = malloc(final_len + 1);
  if (!header) {
    errno = ENOMEM;
    return -1;
  }

  // Actually adding the response header
  snprintf(*header, final_len + 1, header_template, status, content_type,
           content_length);
  *header_size = final_len;

  return 0;
}

// Reads a file into the response buffer, to be called in the
// generate_response()
// Content type and full HTTP header is set here
// 'size' is an output variable that is used to output the final size of the
// response
int read_file(struct client_info *client, const char *alternate_path) {

  FILE *file;
  if (alternate_path == NULL)
    file = fopen(client->request_path, "r");
  else
    file = fopen(alternate_path, "r");

  if (file == NULL)
    return -1;

  fseek(file, 0, SEEK_END); // Seeking to the end of file
  long pos = ftell(file);   // Storing length of file
  rewind(file);             // Rewinding to start to copy the contents

  char *header;
  unsigned int header_size = 0;
  // Final content length will be 'pos' when just serving a file
  // Final content length will be 'pos + *size' showing a directory, *size at
  // this point is just the size of 'dirs'
  if (alternate_path == NULL) {
    if (generate_header(&header, client->response_status,
                        get_mime_type(client->request_path),
                        pos + client->response_len, &header_size) == -1) {
      fclose(file);
      return -1;
    }
  } else {
    if (generate_header(&header, client->response_status,
                        get_mime_type(alternate_path),
                        pos + client->response_len, &header_size) == -1) {
      fclose(file);
      return -1;
    }
  }

  // Response buffer with null-terminator
  client->response = (char *)malloc(pos + header_size);
  client->response[0] = '\0';

  if (!(client->response)) {
    fclose(file);
    errno = ENOMEM; // Errno for malloc errors
    return -1;
  }

  // Adding header to the beginning of the response
  strcat(client->response, header);

  // Number of bytes read, used as size of response
  // Reading to the end of header index
  client->response_len +=
      fread(&((client->response)[header_size]), sizeof(char), pos, file);
  client->response_len += header_size;
  // Lesson learned
  (client->response)[client->response_len] = '\0';

  fclose(file);
  free(header);
  return 0;
}

// Reads the server.html file and fills the response buffer with it, then
// edits the buffer to insert the contents of the directory into it.
int read_directory(struct client_info *client) {

  // Single directory entry
  struct dirent *dir_entry;
  DIR *dir_ptr;
  // Temp array to hold filenames
  char dirs[READ_BUFFER_SIZE];
  dirs[0] = '\0';
  unsigned int dirs_size = 0;

  // Actual directory pointer
  if ((dir_ptr = opendir(client->request_path))) {
    while ((dir_entry = readdir(dir_ptr)) != NULL) {
      // Name of every dir_entry
      char *file_name = dir_entry->d_name;
      // Not adding back and current directories
      if (strcmp(file_name, "..") == 0 || strcmp(file_name, ".") == 0)
        continue;

      dirs_size += strlen(file_name) + strlen("<li>") + strlen("</li>\n");
      strcat(dirs, "<li>");
      strcat(dirs, file_name);

      // Appending a '/' if the entry is a directory itself
      if (dir_entry->d_type == DT_DIR)
        strcat(dirs, "/");
      strcat(dirs, "</li>\n");
    }
  } else
    return -1;

  // New size of response
  client->response_len += dirs_size;

  // Filling the response buffer with the server.html template
  // File only needs to be read here, since it has to be sent the new size,
  // including 'dirs'
  if (read_file(client, "./server.html") == -1)
    return -1;

  char *new_response = malloc(client->response_len);

  // Finding ~ in the html file
  int mark_index;
  for (mark_index = 0; mark_index < client->response_len - dirs_size;
       ++mark_index) {
    if ((client->response)[mark_index] == '~')
      break;
  }
  if (!mark_index) {
    free(new_response);
    return -1;
  }

  // Copying first part into the new response and concatenating dirs
  strncpy(new_response, client->response, mark_index);
  // Not null terminated, terminating to use strcat
  new_response[mark_index] = '\0';
  strcat(new_response, dirs);
  // Appending the remaining file
  strcat(new_response, client->response + mark_index + 1);

  free(client->response);
  client->response = new_response;
  closedir(dir_ptr);
  return 0;
}

// Takes in path, checks if it points to a directory or file, call the
// respective functions to fill the response and sets size of the response
// buffer Pointer to reponse pointer is required to change the response in the
// main function
int generate_response(struct client_info *client) {

  // Metadata of the dir/file
  struct stat request_path_stat;

  if (stat(client->request_path, &request_path_stat) == -1)
    return -1;
  else if (S_ISREG(request_path_stat.st_mode)) { // File
    if (read_file(client, NULL) == -1)
      return -1;
  } else if (S_ISDIR(request_path_stat.st_mode)) { // Directory
    if (read_directory(client) == -1)
      return -1;
  } else {
    errno = EIO;
    return -1;
  }

  return 0;
}

int main(void) {
  // This returns a socket file descriptor as an int, which is like a two way
  // door. This is through which all communication takes place. It takes in
  // three params:
  // 1. Address/Protocol family
  // 2. Socket type (stream or datagram, mainly)
  // 3. Protocol family (0: OS chooses the appropriate one, TCP for stream
  // sockets & UDP for datagram sockets)
  server_fd = socket(AF_INET, SOCK_STREAM, 0);

  // Now the socket has to be binded to an IP & a port, the address should be
  // of any one of the interfaces on this machine. After binding all request
  // to this socket will be routed to that particular IP/port. The address can
  // be: INADDR_ANY: it is used to target all the avaliable interfaces on the
  // machine. Localhost: to target localhost, have to use the loopback address
  // (127.0.0.1) Any IP on the network, also have to convert it to binary form
  // :: is used to bind to all IPv6 addresses.
  // Note: Cannot just assign a string IP address to the server, have to
  // convert it to binary with inet_pton() Also the port has to be  a short in
  // network byte order(big endian), with htons()
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  server_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&server_address,
           sizeof(server_address)) == -1)
    err_n_die("Binding");

  // Now we can start listening with the socket on the ip and port assigned
  if (listen(server_fd, BACKLOG) == -1)
    err_n_die("Listening");

  // Reaping child processes
  struct sigaction sa;
  sa.sa_handler = sigchild_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sa, NULL) == -1)
    err_n_die("Reaping Child Processes");

  struct sigaction sa_shutdown;
  sa_shutdown.sa_handler = shutdown_handler;
  sigemptyset(&sa_shutdown.sa_mask);
  sa_shutdown.sa_flags = 0;

  if (sigaction(SIGINT, &sa_shutdown, NULL) == -1 ||
      sigaction(SIGTERM, &sa_shutdown, NULL) == -1)
    err_n_die("Shutting Down");

  // Loop to accept incoming connections)
  while (running) {
    printf("Server Running at Port: %d\n", PORT);
    // Accepting connections, requires the server_fd and an empty 'struct
    // sockaddr' and its length. On connection, fills it with the address info
    // of the client After accepting, all communication with the said client
    // is done on the new client_fd and server_fd still remains open listening
    // for new conenctions.
    struct client_info new_client;

    new_client.client_fd =
        accept(server_fd, (struct sockaddr *)&new_client.client_address,
               &new_client.address_len);

    if (new_client.client_fd == -1)
      err_n_die("Accepting");

    pid_t pid = fork();
    if (pid == 0) { // Inside Child process

      if (close(server_fd) == -1) // Child should not be listening on server
        err_n_die("Closing Server File Descriptor");

      // Reading and responding
      // Now we can read the request from the client and send any response.
      int bytes_read =
          read(new_client.client_fd, new_client.read_buffer, READ_BUFFER_SIZE);

      if (bytes_read == -1)
        err_n_die("Reading");
      (new_client.read_buffer)[bytes_read] = '\0';

      // Clearing any previous status codes
      memset(new_client.response_status, 0, STATUS_SIZE);
      (new_client.response_status)[0] = '\0';
      memset(root_dir, 0, PATH_SIZE);
      if (set_root_dir() == -1)
        err_n_die("Setting Root Directory");

      if (parse_request(&new_client) == -1)
        err_n_die("Parsing Request");

      if (strncmp(new_client.request_method, "GET", 3) != 0)
        err_n_die("Request Method");

      if (generate_response(&new_client) == -1)
        err_n_die("Generating Response");

      if (write(new_client.client_fd, new_client.response,
                new_client.response_len) == -1)
        err_n_die("Writing Response");

      if (close(new_client.client_fd) == -1)
        err_n_die("Closing Connection");

      free(new_client.response);
      exit(0);
    } else if (pid > 0) {
      if (close(new_client.client_fd) ==
          -1) // Parent does not need client's fd anymore
        err_n_die("Closing Client File Descriptor");
    } else
      err_n_die("Forking");
  }

  printf("Shutting Down\n");

  if (close(server_fd) == -1)
    err_n_die("Closing Server File Descriptor");

  return 0;
}
