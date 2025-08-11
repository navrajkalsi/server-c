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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef STATIC_DIR
#define STATIC_DIR "/usr/local/share/server-c/static"
#endif

#define BACKLOG 10

#define READ_BUFFER_SIZE 4096
#define METHOD_SIZE 10
#define PATH_SIZE 4096

#define STATUS_SIZE 32

volatile sig_atomic_t running = 1;

struct client_info {
  int client_fd;
  struct sockaddr_storage client_address;
  socklen_t address_len;
  char read_buffer[READ_BUFFER_SIZE];
  char request_method[METHOD_SIZE];
  char request_path[PATH_SIZE];
  char *response;
  size_t response_len;
  char response_status[STATUS_SIZE];
};

int server_fd;
struct sockaddr_in server_address;

char *SUPPORTED_METHODS[] = {"GET"};

char root_dir[PATH_SIZE] = "\0";
uint16_t PORT = 1419;

int DEBUG = 0;

in_addr_t client_addr_t = INADDR_LOOPBACK;

void err_n_die(const char *operation) {
  printf("%s failed!\n", operation);
  printf("Error Code: %d\n", errno);
  printf("Error Message: %s\n\n", strerror(errno));
  exit(EXIT_FAILURE);
}

void shutdown_handler(int s) {
  (void)s;
  running = 0;
  return;
}

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

void sigchild_handler(int s) {
  (void)s;

  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

void print_debug(const char *msg) {
  if (DEBUG == 1)
    puts(msg);
  return;
}

void parse_args(int argc, char *argv[]) {

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

  int arg;

  int args_parsed = 0;
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
      PORT = (uint16_t)atoi(optarg);

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
    case '?':

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

int set_root_dir(void) {
  if (!realpath("./", root_dir))
    return -1;
  if (DEBUG == 1)
    printf("Root Directory set to: %s\n", root_dir);
  return 0;
}

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

int simplify_url(struct client_info *client) {

  if (strcmp(client->request_path, "/") == 0) {
    strcpy(client->request_path, "./");
    return 0;
  }

  char *charPtr;

  while ((charPtr = strstr(client->request_path, "%20"))) {

    *charPtr = ' ';

    charPtr++;

    while ((*charPtr = *(charPtr + 2)))
      charPtr++;
  }
  return 0;
}

int parse_request(struct client_info *client) {

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

    char *not_implemented_response = "HTTP/1.1 501 Not Implemented\r\n\r\n";
    write(client->client_fd, not_implemented_response,
          strlen(not_implemented_response));
    errno = ENOTSUP;
    print_debug("Request Method Not Supported.\n");
    return -1;
  }

  print_debug("Request Method is Supported.\n");

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

  while (client->request_path[0] == '/' && client->request_path[1] != '\0')
    memmove(client->request_path, client->request_path + 1,
            strlen(client->request_path));

  if (is_path_static == 0) {

    char fullpath[PATH_SIZE];

    strncpy(fullpath, root_dir, PATH_SIZE);
    strcat(fullpath, "/");
    strcat(fullpath, client->request_path);

    if (!realpath(fullpath, client->request_path)) {

      if (errno == 2) {
        strncpy(client->request_path, "/404.html", PATH_SIZE);
        snprintf(client->response_status, STATUS_SIZE, "404 Not Found");
        is_path_static = 1;
        puts("break\n");
      } else
        return -1;
    } else {

      if (strncmp(root_dir, client->request_path, strlen(root_dir)) != 0) {
        errno = EPERM;
        return -1;
      }
    }
  }

  if (is_path_static == 1) {

    char temp_dir[PATH_SIZE] = STATIC_DIR;
    strncat(temp_dir, "/", PATH_SIZE - (strlen(temp_dir)));
    strncat(temp_dir, client->request_path, PATH_SIZE - (strlen(temp_dir)));
    strncpy(client->request_path, temp_dir, PATH_SIZE);
  }

  if (is_path_static == -1)
    return -1;

  return 0;
}

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

int generate_header(char **header, char *status, const char *content_type,
                    unsigned int content_length, size_t *header_size) {
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

  size_t final_len = (size_t)snprintf(NULL, 0, header_template, status,
                                      content_type, content_length);
  *header = malloc(++final_len);
  if (!*header) {
    errno = ENOMEM;
    return -1;
  }

  snprintf(*header, final_len, header_template, status, content_type,
           content_length);
  *header_size = --final_len;

  return 0;
}

int read_file(struct client_info *client, const char *alternate_path) {
  FILE *file;
  if (alternate_path == NULL)
    file = fopen(client->request_path, "r");
  else
    file = fopen(alternate_path, "r");

  if (file == NULL)
    return -1;

  fseek(file, 0, SEEK_END);
  long file_len = ftell(file);
  if (file_len == -1L)
    return -1;
  rewind(file);

  char *header;
  size_t header_size = 0;
  char *mime = NULL;

  if (alternate_path == NULL) {

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
                        (unsigned int)file_len +
                            (unsigned int)client->response_len,
                        &header_size) == -1) {
      fclose(file);
      free(mime);
      return -1;
    }
  } else {
    mime = get_mime_type(alternate_path);
    if (!mime)
      return -1;
    if (generate_header(&header, client->response_status, mime,
                        (unsigned int)file_len +
                            (unsigned int)client->response_len,
                        &header_size) == -1) {
      fclose(file);
      free(mime);
      return -1;
    }
  }
  free(mime);

  client->response = (char *)malloc((size_t)file_len + header_size + 1);

  if (!(client->response)) {
    fclose(file);
    errno = ENOMEM;
    return -1;
  }

  client->response[0] = '\0';

  strcat(client->response, header);

  client->response_len += fread(&((client->response)[header_size]),
                                sizeof(char), (size_t)file_len, file);
  client->response_len += header_size;

  (client->response)[(size_t)file_len + header_size] = '\0';

  fclose(file);
  free(header);
  return 0;
}

int read_directory(struct client_info *client) {

  struct dirent *dir_entry;
  DIR *dir_ptr;

  char dirs[READ_BUFFER_SIZE];
  dirs[0] = '\0';
  size_t dirs_size = 0;

  if ((dir_ptr = opendir(client->request_path))) {
    while ((dir_entry = readdir(dir_ptr)) != NULL) {

      char *file_name = dir_entry->d_name;

      if (strcmp(file_name, "..") == 0 || strcmp(file_name, ".") == 0)
        continue;

      dirs_size += strlen(file_name) + strlen("<li>") + strlen("</li>\n");
      strcat(dirs, "<li>");
      strcat(dirs, file_name);

      if (dir_entry->d_type == DT_DIR) {
        dirs_size += 1;
        strcat(dirs, "/");
      }
      strcat(dirs, "</li>\n");
    }
  } else
    return -1;

  client->response_len += dirs_size - 1;

  char temp_dir[PATH_SIZE] = STATIC_DIR;
  strncat(temp_dir, "/", PATH_SIZE - (strlen(temp_dir)));

  if (read_file(client, strncat(temp_dir, "server.html",
                                PATH_SIZE - strlen(temp_dir))) == -1)
    return -1;

  char *new_response = malloc(client->response_len + 1);

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

  strncpy(new_response, client->response, mark_index);

  new_response[mark_index] = '\0';

  strcat(new_response, dirs);

  strcat(new_response, client->response + mark_index + 1);

  free(client->response);
  client->response = new_response;
  closedir(dir_ptr);
  return 0;
}

int generate_response(struct client_info *client) {

  struct stat request_path_stat;

  if (stat(client->request_path, &request_path_stat) == -1)
    return -1;
  else if (S_ISREG(request_path_stat.st_mode)) {
    if (read_file(client, NULL) == -1)
      return -1;
  } else if (S_ISDIR(request_path_stat.st_mode)) {
    if (read_directory(client) == -1)
      return -1;
  } else {
    errno = EIO;
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  parse_args(argc, argv);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    err_n_die("Creating Socket");
  print_debug("Socket Created.\n");

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

  if (listen(server_fd, BACKLOG) == -1)
    err_n_die("Listening");

  printf("Server Listening at Port: %d\n", PORT);

  struct sigaction sa_reap;

  sa_reap.sa_handler = sigchild_handler;

  sigemptyset(&sa_reap.sa_mask);
  sa_reap.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sa_reap, NULL) == -1)
    err_n_die("Reaping Child Processes");

  struct sigaction sa_shutdown;
  sa_shutdown.sa_handler = shutdown_handler;
  sigemptyset(&sa_shutdown.sa_mask);
  sa_shutdown.sa_flags = 0;

  if (sigaction(SIGINT, &sa_shutdown, NULL) == -1 ||
      sigaction(SIGTERM, &sa_shutdown, NULL) == -1)
    err_n_die("Shutting Down");

  while (running == 1) {

    struct client_info new_client;
    new_client.response = NULL;
    new_client.response_len = 0;

    if ((new_client.client_fd =
             accept(server_fd, (struct sockaddr *)&new_client.client_address,
                    &new_client.address_len)) == -1) {

      if (errno == EINTR && !running)
        break;
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

    if (pid == 0) {
      print_debug("Inside Child Process.\n");

      if (close(server_fd) == -1)
        err_n_die("Closing Server File Descriptor");

      print_debug("Closed Parent Server File Descriptor.\n");

      long int bytes_read =
          read(new_client.client_fd, new_client.read_buffer, READ_BUFFER_SIZE);

      if (bytes_read == -1) {
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Reading");
      }
      (new_client.read_buffer)[bytes_read] = '\0';
      print_debug("Incoming Request Read.\n");

      memset(new_client.response_status, 0, STATUS_SIZE);
      (new_client.response_status)[0] = '\0';

      if (strlen(root_dir) == 0) {

        if (set_root_dir() == -1) {
          if (errno == EINTR && !running)
            break;
          else
            err_n_die("Setting Root Directory");
        }
        print_debug("-r Option not used.\nRoot Directory set to default.\n");
      }

      if (parse_request(&new_client) == -1 && errno != ENOENT) {
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Parsing Request");
      }
      print_debug("Parsed Incoming Request.\n");

      if (generate_response(&new_client) == -1) {
        if (errno == EINTR && !running)
          break;
        else
          err_n_die("Generating Response");
      }
      print_debug("Response Generated.\n");

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
