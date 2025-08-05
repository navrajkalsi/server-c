#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <bits/getopt_core.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <magic.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Need to compile with -lmagic flag to use magic.h for get_mime_type()

// Dir to house the static files for the server
// Can be passed by the user when compiling manually, without make
// Just for static files, bin may or may not be in the same dir, most likely not
#ifndef STATIC_DIR
#define STATIC_DIR "/usr/local/share/server-c/static"
#endif

// Macros
#define BACKLOG 10
// Request related
#define READ_BUFFER_SIZE 4096
#define METHOD_SIZE 10
#define PATH_SIZE 4096
// Response related
#define STATUS_SIZE 32

// Variable to determine running status of server
// Used for shutting down server with SIGTERM/SIGINT
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

// Supported methods for the server
char *SUPPORTED_METHODS[] = {"GET"};

// Root dir of server and port
char root_dir[PATH_SIZE] = "\0";
int PORT = 1419;

// Pass -d flag to use debug mode, prints every activity to the terminal
int DEBUG = 0;

// Pass -a to accept incoming connections from all IPs
// By default, accepts request only from localhost
in_addr_t client_addr_t = INADDR_LOOPBACK;

void err_n_die(const char *operation) {
  printf("%s failed!\n", operation);
  printf("Error Code: %d\n", errno);
  printf("Error Message: %s\n\n", strerror(errno));
  exit(EXIT_FAILURE);
}

// Exits the program by turning the loop condition to false,
// and then in main() memory is cleaned
void shutdown_handler(int s) {
  (void)s;
  running = 0;
  return;
}

// Check if the method is supported, is present in SUPPORTED_METHODS
// Returns 1 if the method is valid, 0 is not, and -1 in case of error
int is_method_valid(const char *method) {
  if (!method)
    return -1;

  unsigned int methods_len =
      sizeof(SUPPORTED_METHODS) / sizeof(SUPPORTED_METHODS[0]);

  if (DEBUG == 1) {
    puts("Supported Request Methods are:\n");
    for (unsigned int i = 0; i < methods_len; ++i)
      printf("%d. %s\n", i + 1, SUPPORTED_METHODS[i]);
  }

  for (unsigned int i = 0; i < methods_len; ++i)
    if (strcmp(method, SUPPORTED_METHODS[i]) == 0)
      return 1;

  return 0;
}

// Reaps all child processes and does not let them become zombie processes as
// they occupy process table slots
void sigchild_handler(int s) {
  (void)s;

  // waitpid might change the errno
  int saved_errno = errno;

  // waitpid() returns the PID of the exited child
  // Takes in:
  // PID of child (-1 targets every child)
  // output param to set the exit status of the child to
  // WNOHANG means non-blocking
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

// Prints passed message to the console, if debug flag/option is on
void print_debug(const char *msg) {
  if (DEBUG == 1)
    puts(msg);
  return;
}

// Parses args from the command line, if any
// Errors and exits if the root_dir passed does not exist
void parse_args(int argc, char *argv[]) {
  // Looking for -h flag before scanning for other flags, to make sure I do not
  // start the server if -h is used
  for (int i = 1; i < argc; i++)
    if (strcmp(argv[i], "-h") == 0) {
      printf(
          "Usage: %s [OPTIONS] [ARGS...]\n"
          "Options:\n"
          "-a             Accept Incoming Connections from all IPs, defaults "
          "to Localhost only.\n"
          "-d             Debug Mode, prints every major function call.\n"
          "-h             Print this help message.\n"
          "-p <port>      Port to listen on.\n"
          "-r <directory> Directory to serve.\n",
          argv[0]);
      exit(EXIT_SUCCESS);
    };

  int arg; // cannot be char, although the switch will compare it to char,
           // because getopt() can return -1 as well, therefore we will be
           // comparing the ASCII values of char literals

  // ':' is required to tell if the flag requires an argument after the flag in
  // cmd line
  int args_parsed = 0; // For debugging
  while ((arg = getopt(argc, argv, "adhp:r:")) != -1) {
    switch (arg) {
    case 'd':
      DEBUG = 1;
      args_parsed++;
      puts("Debug Mode On.\n");
      break;
    case 'a':
      client_addr_t = INADDR_ANY;
      args_parsed++;
      break;
    case 'p':
      PORT = atoi(optarg); // 'optarg' is a global variable set by getopt()
      // Have to convert it from ASCII string to integer
      // with atoi()
      args_parsed++;
      if (DEBUG == 1)
        printf("Port changed to: %d\n", PORT);
      break;
    case 'r':
      if (!realpath(optarg, root_dir))
        err_n_die("Setting Root Directory");
      args_parsed++;
      if (DEBUG == 1)
        printf("Root Directory set to: %s\n", root_dir);
      break;
    case '?': // If an unknown flag or no argument is passed for an option
              // 'optopt' is set to the flag
      if (optopt == 'p')
        puts("Option '-p' requires passing a valid port number\nUse '-h' for "
             "usage.\n");
      else if (optopt == 'r')
        puts(
            "Option '-r' requries passing a valid directory path\nUse '-h' for "
            "usage.\n");
      else if (isprint(optopt))
        printf("Unknown option: '-%c'.\n", optopt);
      else
        puts("Unknown option character used!\n");
      exit(EXIT_FAILURE);
    default:
      puts("Unknown error occurred while parsing arguments\n");
      exit(EXIT_FAILURE);
    }
  }

  if (DEBUG == 1)
    printf("Parsed %d Argument(s).\n\n", args_parsed);
  return;
}

// Sets global variable 'root_dir' to the working directory
int set_root_dir(void) {
  if (!realpath("./", root_dir))
    return -1;
  if (DEBUG == 1)
    printf("Root Directory set to: %s\n", root_dir);
  return 0;
}

// Handles requesting of any static files (currently includes:
// /favicon.ico, /server.js, /server.html, /404.html
// Returns 1 if the path has to be dealt with statically and not to be used
// with realpath() in parse_request()
int check_static_request(struct client_info *client) {
  if (!client)
    return -1;

  if (strcmp(client->request_path, "/favicon.ico") == 0)
    return 1;
  if (strcmp(client->request_path, "/server.js") == 0)
    return 1;
  if (strcmp(client->request_path, "/server.html") == 0)
    return 1;
  if (strcmp(client->request_path, "/404.html") == 0)
    return 1;

  return 0;
}

// Makes url usable in c, as the request url may be encoded
// Converts '/' to './'
// Converts '%20' to ' '
// Add anymore url alternations here
int simplify_url(struct client_info *client) {
  // An array of search terms could be implemented if there more string that
  // need to be replaced

  if (strcmp(client->request_path, "/") == 0) {
    strcpy(client->request_path, "./");
    return 0; // No need to check %20 now
  }

  char *charPtr;
  // Replacing terms that need to be replaced
  while ((charPtr = strstr(client->request_path, "%20"))) {
    // To start, charPtr points to '%'
    *charPtr = ' '; // Although there will still be data that was written
                    // after null-terminator, but I own the memory
    charPtr++;      // Pointing to '2'

    // Moving chars over
    while ((*charPtr = *(charPtr + 2)))
      charPtr++;
  }
  return 0;
}

// Parses a request, extracting the 'request_method' & 'request_path'.
// Request path is converted to absolute path and checked for traversal
int parse_request(struct client_info *client) {
  // Change safe limits here based on max method and path sizes
  // 1 less because of null pointer
  // Also change the number of returned args in case more args are required in
  // future
  if (sscanf(client->read_buffer, "%9s %4095s", client->request_method,
             client->request_path) != 2)
    return -1;

  print_debug("Parsing Request.\n");

  if (DEBUG == 1)
    printf("Received Request Path: %s\nReceived Request Method: %s\n",
           client->request_path, client->request_method);

  int method_valid = is_method_valid(client->request_method);
  if (method_valid == -1) {
    print_debug("Request Method Checking Failed.\n");
    return -1;
  } else if (method_valid == 0) {
    // Responding with 501 Not Implemented
    char *not_implemented_response = "HTTP/1.1 501 Not Implemented\r\n\r\n";
    write(client->client_fd, not_implemented_response,
          strlen(not_implemented_response));
    errno = ENOTSUP;
    print_debug("Request Method Not Supported.\n");
    return -1;
  }

  print_debug("Request Method is Supported.\n");

  // Taking out any '%20's
  // When user requests for '/', the server should serve pwd
  if (simplify_url(client) == -1)
    return -1;
  if (DEBUG == 1)
    printf("Simplified Request Path: %s\n", client->request_path);

  char client_ip[INET6_ADDRSTRLEN] = {0};

  getnameinfo((struct sockaddr *)&client->client_address, client->address_len,
              client_ip, sizeof(client_ip), NULL, 0, NI_NUMERICHOST);

  printf("(%s) %s %s\n\n", client_ip, client->request_method,
         client->request_path);

  int is_path_static = check_static_request(client);

  // Removing beginning '/'s to be able to use realpath()
  while (client->request_path[0] == '/' && client->request_path[1] != '\0')
    memmove(client->request_path, client->request_path + 1,
            strlen(client->request_path));

  if (is_path_static == 0) { // Absolute path is to be used
    // Thanks Prof Kevin Forest!
    // Getting actual absolute path of the target

    // Have to append the root_dir to request_path, because if root_dir is set
    // with args the realpath() still considers the pwd as the root dir
    char fullpath[PATH_SIZE]; // This path may be illegal to use, but that
                              // will be sorted later
    strncpy(fullpath, root_dir, PATH_SIZE); // Starting the path with root dir
    strcat(fullpath, "/");
    strcat(fullpath, client->request_path); // Now the path will be root(set
                                            // by user)/requested_file
    // This will work even if the user does not use -r flag, as set_root_dir()
    // will set the root_dir to pwd
    if (!realpath(fullpath, client->request_path)) {
      // If the errno is set to 2, that means the requested directory/file
      // does not exist Then the server serves the '404.html file instead'
      if (errno == 2) {
        strncpy(client->request_path, "/404.html", PATH_SIZE);
        snprintf(client->response_status, STATUS_SIZE,
                 "404 Not Found"); // Setting status code
        is_path_static = 1;
        puts("break\n");
      } else
        return -1;
    } else {
      // Comparing the starting chars of the 'root_dir' and 'request_path' to
      // prevent any path traversal
      if (strncmp(root_dir, client->request_path, strlen(root_dir)) != 0) {
        errno = EPERM;
        return -1;
      }
    }
  }

  // Not using else if, to serve 404.html if absolute path does not exist
  if (is_path_static == 1) {
    // If the request is for a static file from STATIC_DIR it will be made
    // after prepending the STATIC_DIR
    char temp_dir[PATH_SIZE] = STATIC_DIR;
    strncat(temp_dir, "/", PATH_SIZE - (strlen(temp_dir)));
    strncat(temp_dir, client->request_path, PATH_SIZE - (strlen(temp_dir)));
    strncpy(client->request_path, temp_dir, PATH_SIZE);
  }

  if (is_path_static == -1)
    return -1;

  return 0;
}

// Returns the mime type of a file
// MIME type for '/server.js' is set to 'application/javascript' to prevent
// browser warnings, look in the read_file()
// Rest of the js files will be served with type of
// 'text/plain' for easy preview
char *get_mime_type(const char *filepath) {
  magic_t magic = magic_open(MAGIC_MIME_TYPE);
  if (!magic)
    return NULL;

  if (magic_load(magic, NULL) != 0) {
    magic_close(magic);
    return NULL;
  }

  const char *mime = magic_file(magic, filepath);
  if (!mime) {
    magic_close(magic);
    return NULL;
  }

  char *mime_return = strdup(mime);
  magic_close(magic);

  return mime_return;
}

// Generates an HTTP response header
// Defaults to 200, if status is empty
int generate_header(char **header, char *status, const char *content_type,
                    unsigned int content_length, unsigned int *header_size) {
  if (status[0] == '\0')
    snprintf(status, STATUS_SIZE, "200 OK");

  const char *header_template =
      "HTTP/1.1 %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Access-Control-Expose-Headers: Content-Type\r\n"
      "\r\n";

  int final_len = snprintf(NULL, 0, header_template, status, content_type,
                           content_length); // calculating just the final length
  *header = malloc(final_len + 1);
  if (!*header) {
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

  fseek(file, 0, SEEK_END);    // Seeking to the end of file
  long file_len = ftell(file); // Storing length of file
  rewind(file);                // Rewinding to start to copy the contents

  char *header;
  unsigned int header_size = 0;
  char *mime = NULL;
  // Final content length will be 'file_len' when just serving a file
  // Final content length will be 'file_len + *size' showing a directory,
  // *size at this point is just the size of 'dirs'
  if (alternate_path == NULL) {
    //
    // Alter code here to add more custom MIME types for static files
    //
    char temp_dir[PATH_SIZE] = STATIC_DIR;
    strncat(temp_dir, "/", PATH_SIZE - (strlen(temp_dir)));
    if (strcmp(client->request_path,
               strncat(temp_dir, "server.js", PATH_SIZE - strlen(temp_dir))) ==
        0)
      mime = strdup("application/javascript");
    else
      mime = get_mime_type(client->request_path);

    if (!mime)
      return -1;
    if (generate_header(&header, client->response_status, mime,
                        file_len + client->response_len, &header_size) == -1) {
      fclose(file);
      free(mime);
      return -1;
    }
  } else {
    mime = get_mime_type(alternate_path);
    if (!mime)
      return -1;
    if (generate_header(&header, client->response_status, mime,
                        file_len + client->response_len, &header_size) == -1) {
      fclose(file);
      free(mime);
      return -1;
    }
  }
  free(mime);

  // Response buffer with null-terminator
  client->response = (char *)malloc(file_len + header_size + 1);

  if (!(client->response)) {
    fclose(file);
    errno = ENOMEM; // Errno for malloc errors
    return -1;
  }

  client->response[0] = '\0';

  // In case of reading a directory, response_len already has the 'dirs_size'
  // Response started building from here
  // Adding header to the beginning of the response
  strcat(client->response, header);

  // Number of bytes read, used as size of response
  // Reading to the end of header index
  client->response_len +=
      fread(&((client->response)[header_size]), sizeof(char), file_len, file);
  client->response_len += header_size;

  // Lesson learned
  // And not using client.response_len as the null terminator position, as it
  // may also contain the dirs_size from read_directory() but not the actual
  // directory data, therefore we get garbage data at the end if I try to
  // print the resposne here. Plus it leads to 'double free' error.
  (client->response)[file_len + header_size] = '\0';

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
      if (dir_entry->d_type == DT_DIR) {
        dirs_size += 1;
        strcat(dirs, "/");
      }
      strcat(dirs, "</li>\n");
    }
  } else
    return -1;

  // New size of response
  // -1 because we will be removing ~ from the html file
  client->response_len += dirs_size - 1;

  // Filling the response buffer with the server.html template
  // File only needs to be read here, since it has to be sent the new size,
  // including 'dirs'
  char temp_dir[PATH_SIZE] = STATIC_DIR;
  strncat(temp_dir, "/", PATH_SIZE - (strlen(temp_dir)));

  if (read_file(client, strncat(temp_dir, "server.html",
                                PATH_SIZE - strlen(temp_dir))) == -1)
    return -1;

  char *new_response = malloc(client->response_len + 1);

  // Finding ~ in the html file
  unsigned int mark_index;
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

int main(int argc, char *argv[]) {
  parse_args(argc,
             argv); // PORT, root_dir & DEBUG will be set, if passed by user

  // This returns a socket file descriptor as an int, which is like a two way
  // door. This is through which all communication takes place. It takes in
  // three params:
  // 1. Address/Protocol family
  // 2. Socket type (stream or datagram, mainly)
  // 3. Protocol family (0: OS chooses the appropriate one, TCP for stream
  // sockets & UDP for datagram sockets)
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    err_n_die("Creating Socket");
  print_debug("Socket Created.\n");

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
  server_address.sin_addr.s_addr = htonl(client_addr_t);

  if (server_address.sin_addr.s_addr == htonl(INADDR_ANY))
    puts("Server Accepting Incoming Connections from all IPs.\n");
  else if (server_address.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
    puts("Server Accepting Incoming Connections from Localhost Only.\n");
  else
    err_n_die("Binding");

  if (bind(server_fd, (struct sockaddr *)&server_address,
           sizeof(server_address)) == -1)
    err_n_die("Binding");
  print_debug("Socket Binded to the port.\n");

  // Now we can start listening with the socket on the ip and port assigned
  if (listen(server_fd, BACKLOG) == -1)
    err_n_die("Listening");

  printf("Server Listening at Port: %d\n", PORT);

  // Child processes will be creating next.
  // When a child exits, it is a zomibe process until its exit status is read
  // by the parent (reaping) Reaping child processes
  struct sigaction sa_reap;
  // The assigned function will automatically be called when any signal is
  // received from a child
  sa_reap.sa_handler = sigchild_handler;
  // Clears any signals that are set by default to be blocked
  sigemptyset(&sa_reap.sa_mask);
  sa_reap.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sa_reap, NULL) == -1)
    err_n_die("Reaping Child Processes");

  // Handling shutdown
  struct sigaction sa_shutdown;
  sa_shutdown.sa_handler = shutdown_handler;
  sigemptyset(&sa_shutdown.sa_mask);
  sa_shutdown.sa_flags = 0; // No flags required for shutting down

  // SIGINT (signal interput) is sent when Ctrl+C is pressed
  // SIGTERM (signal terminate) is sent when the process is killed from like
  // terminal with kill command
  if (sigaction(SIGINT, &sa_shutdown, NULL) == -1 ||
      sigaction(SIGTERM, &sa_shutdown, NULL) == -1)
    err_n_die("Shutting Down");

  // Loop to accept incoming connections
  while (running == 1) {
    // Accepting connections, requires the server_fd and an empty 'struct
    // sockaddr' and its length. On connection, fills it with the address info
    // of the client After accepting, all communication with the said client
    // is done on the new client_fd and server_fd still remains open listening
    // for new conenctions.
    struct client_info new_client;
    new_client.response = NULL;
    new_client.response_len = 0;

    if ((new_client.client_fd =
             accept(server_fd, (struct sockaddr *)&new_client.client_address,
                    &new_client.address_len)) == -1) {
      // Checking how the accept method failed
      // If errno == EINTR, it means the process was interrupted and the loop
      // needs to break, in order to shutdown the server Otherwise something
      // else is wrong, and err_n_die is used to handle that
      // Have to do this for every error handling inside the while loop
      if (errno == EINTR && !running)
        break; // Breaking loop shuts the server down.
      else
        err_n_die("Accepting");
    }

    pid_t pid;

    if ((pid = fork()) == -1) {
      if (errno == EINTR && !running)
        break;
      else
        err_n_die("Forking");
    }
    print_debug("Forked.\n");

    if (pid == 0) { // Inside Child process
      print_debug("Inside Child Process.\n");

      if (close(server_fd) == -1) // Child should not be listening on server
        err_n_die("Closing Server File Descriptor");

      print_debug("Closed Parent Server File Descriptor.\n");

      // Reading and responding
      // Now we can read the request from the client and send any response.
      int bytes_read =
          read(new_client.client_fd, new_client.read_buffer, READ_BUFFER_SIZE);

      if (bytes_read == -1) {
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Reading");
      }
      (new_client.read_buffer)[bytes_read] = '\0';
      print_debug("Incoming Request Read.\n");

      // Clearing any previous status codes
      memset(new_client.response_status, 0, STATUS_SIZE);
      (new_client.response_status)[0] = '\0';

      // Setting Root Dir
      if (strlen(root_dir) == 0) {
        // If -r flag was not used, then the root_dir
        // is not set yet and will have len of 0
        if (set_root_dir() == -1) {
          if (errno == EINTR && !running)
            break;
          else
            err_n_die("Setting Root Directory");
        }
        print_debug("-r Option not used.\nRoot Directory set to default.\n");
      }

      // Parse Request
      if (parse_request(&new_client) == -1 && errno != ENOENT) {
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Parsing Request");
      }
      print_debug("Parsed Incoming Request.\n");

      // Generating Response
      if (generate_response(&new_client) == -1) {
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Generating Response");
      }
      print_debug("Response Generated.\n");

      // Writing Response
      if (write(new_client.client_fd, new_client.response,
                new_client.response_len) == -1) {
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Writing Response");
      }
      print_debug("Response Written to the Client File Descriptor.\n");

      if (close(new_client.client_fd) == -1) {
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Closing Connection");
      }
      print_debug("Connection Closed.\n");

      free(new_client.response);
      print_debug("Response Freed.\nExiting...\n");
      exit(0);
    } else if (pid > 0) {
      print_debug("Inside Parent Process.\n");

      if (close(new_client.client_fd) == -1) {
        // Parent does not need client's fd anymore
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Closing Client File Descriptor");
      }

      print_debug("Closed Child Client File Descriptor.\n");
    }
  }

  printf("\nShutting Down...\n");

  if (close(server_fd) == -1)
    err_n_die("Closing Server File Descriptor");

  print_debug("Closed Server File Descriptor.\n");

  return 0;
}
