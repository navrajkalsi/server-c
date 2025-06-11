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

#define PORT 1419
#define BACKLOG 10

#define READ_BUFFER_SIZE 4096
#define METHOD_SIZE 10
#define PATH_SIZE 4096

volatile sig_atomic_t running = 1;

struct client_info {
  int client_fd;
  struct sockaddr client_address;
};

int server_fd;
struct sockaddr_in server_address;

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

  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

int set_root_dir(void) {
  if (!realpath("./", root_dir))
    return -1;
  return 0;
}

int parse_request(const char *request, char *request_method,
                  char *request_path) {

  printf("Received Request: \n\n%s", request);
  if (sscanf(request, "%9s %4095s", request_method, request_path) != 2)
    return -1;

  if (request_path[0] == '/')
    request_path += 1;

  if (!realpath(request_path, request_path))
    return -1;

  printf("Request Method: %s\n", request_method);
  printf("Request Path: %s\n", request_path);

  if (strncmp(root_dir, request_path, strlen(root_dir)) != 0) {
    errno = EPERM;
    return -1;
  }

  return 0;
}

const char *get_mime_type(const char *filepath) {
  magic_t magic = magic_open(MAGIC_MIME_TYPE);
  if (magic == NULL)
    return NULL;
  if (magic_load(magic, NULL) != 0) {
    magic_close(magic);
    return NULL;
  }

  const char *mime = magic_file(magic, filepath);
  return mime;
}

int generate_header(char **header, const char *status, const char *content_type,
                    unsigned int content_length, unsigned int *header_size) {

  const char *header_template =
      "HTTP/1.1 %s\r\n Content-Type: %s\r\n Content-Length: %u\r\n Connection: "
      "close\r\n\r\n";

  int final_len =
      snprintf(NULL, 0, header_template, status, content_type, content_length);
  *header = malloc(final_len + 1);
  if (!header) {
    errno = ENOMEM;
    return -1;
  }

  snprintf(*header, final_len + 1, header_template, status, content_type,
           content_length);
  *header_size = final_len;
  return 0;
}

int read_file(const char *request_path, char **response, unsigned int *size) {

  FILE *file = fopen(request_path, "r");

  if (file == NULL)
    return -1;

  fseek(file, 0, SEEK_END);
  long pos = ftell(file);
  rewind(file);

  char *header;
  unsigned int header_size = 0;

  if (generate_header(&header, "200 OK", get_mime_type(request_path),
                      pos + *size, &header_size) == -1)
    return -1;

  *response = (char *)malloc(pos + header_size);
  *response[0] = '\0';

  if (!*response) {
    fclose(file);
    errno = ENOMEM;
    return -1;
  }

  strcat(*response, header);

  *size += fread(&((*response)[header_size - 1]), sizeof(char), pos, file);
  *size += header_size;

  (*response)[*size] = '\0';

  fclose(file);
  free(header);
  return 0;
};

int read_directory(const char *request_path, char **response,
                   unsigned int *size) {

  struct dirent *dir_entry;
  DIR *dir_ptr;

  char dirs[READ_BUFFER_SIZE];
  dirs[0] = '\0';
  unsigned int dirs_size = 0;

  if ((dir_ptr = opendir(request_path))) {
    while ((dir_entry = readdir(dir_ptr)) != NULL) {

      char *file_name = dir_entry->d_name;

      if (strcmp(file_name, "..") == 0 || strcmp(file_name, ".") == 0)
        continue;
      dirs_size += strlen(file_name) + strlen("<li>") + strlen("</li>\n");
      strcat(dirs, "<li>");
      strcat(dirs, file_name);
      strcat(dirs, "</li>\n");
    }
  } else
    return -1;

  *size += dirs_size;

  if (read_file("./server.html", response, size) == -1)
    return -1;

  char *new_response = malloc(*size);

  int mark_index;
  for (mark_index = 0; mark_index < *size - dirs_size; ++mark_index) {
    if ((*response)[mark_index] == '~')
      break;
  }
  if (!mark_index) {
    free(new_response);
    return -1;
  }

  strncpy(new_response, *response, mark_index);

  new_response[mark_index] = '\0';
  strcat(new_response, dirs);

  strcat(new_response, *response + mark_index + 1);

  free(*response);
  *response = new_response;
  closedir(dir_ptr);
  return 0;
};

int generate_response(const char *request_path, char **response,
                      unsigned int *size) {

  struct stat request_path_stat;

  if (stat(request_path, &request_path_stat) == -1)
    return -1;
  else if (S_ISREG(request_path_stat.st_mode)) {
    if (read_file(request_path, response, size) == -1)
      return -1;
  } else if (S_ISDIR(request_path_stat.st_mode)) {
    if (read_directory(request_path, response, size) == -1)
      return -1;
  } else {
    errno = EIO;
    return -1;
  }

  return 0;
}

int main(void) {

  server_fd = socket(AF_INET, SOCK_STREAM, 0);

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  server_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&server_address,
           sizeof(server_address)) == -1)
    err_n_die("Binding");

  if (listen(server_fd, BACKLOG) == -1)
    err_n_die("Listening");

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

  while (running) {

    struct client_info new_client;
    struct sockaddr client_address;
    socklen_t add_len = sizeof(client_address);

    int client_fd = accept(server_fd, &client_address, &add_len);

    if (client_fd == -1)
      err_n_die("Accepting");

    pid_t pid = fork();
    if (pid == 0) {

      if (close(server_fd) == -1)
        err_n_die("Closing Server File Descriptor");

      char read_buffer[READ_BUFFER_SIZE];
      char request_method[METHOD_SIZE];
      char request_path[PATH_SIZE];

      memset(&read_buffer, 0, READ_BUFFER_SIZE);

      char *response = NULL;
      unsigned int response_len = 0;

      new_client.client_fd = client_fd;
      new_client.client_address = client_address;

      int bytes_read =
          read(new_client.client_fd, read_buffer, READ_BUFFER_SIZE);

      if (bytes_read == -1)
        err_n_die("Reading");
      read_buffer[bytes_read] = '\0';

      memset(root_dir, 0, PATH_SIZE);
      if (set_root_dir() == -1)
        err_n_die("Setting Root Directory");

      if (parse_request(&read_buffer[0], &request_method[0],
                        &request_path[0]) == -1)
        err_n_die("Parsing Request");

      if (generate_response(&request_path[0], &response, &response_len) == -1)
        err_n_die("Generating Response");

      printf("Reponse: %s\n", response);

      if (write(new_client.client_fd, response, response_len) == -1)
        err_n_die("Writing Response");

      if (close(new_client.client_fd) == -1)
        err_n_die("Closing Connection");

      free(response);
      exit(0);
    } else if (pid > 0) {
      if (close(client_fd) == -1)
        err_n_die("Closing Client File Descriptor");
    } else
      err_n_die("Forking");
  }

  printf("Shutting Down\n");

  if (close(server_fd) == -1)
    err_n_die("Closing Server File Descriptor");

  return 0;
}
