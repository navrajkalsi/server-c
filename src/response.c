#include "../include/response.h"
#include "../include/request.h"
#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Response bodies are the only malloced vars in this whole program

int handle_response(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  print_client(client);
  // Setting both response structs to null
  client->dynamic_response_body = ERR_STR;
  client->static_response_body = ERR_STR;

  // If response code not set, something is not right
  if (!client->response_status.data || !client->response_status.len)
    client->response_status = STR("500 Internal Server Error");

  // If response is not OK, then print the error message on client side
  if (equals(&client->response_status, &STR("200 OK")))
    if (generate_response(client) < 0)
      return err("Generating response", true);

  if (write_response(client) < 0)
    return err("Writing response", true);

  return 0;
}

int write_response(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  if (client->static_response_body.len && client->static_delimiter++) {
    Str before_delimiter, after_delimiter;
    before_delimiter = after_delimiter = client->static_response_body;

    before_delimiter.len = client->static_delimiter - 1;
    after_delimiter.data += client->static_delimiter;
    after_delimiter.len -= client->static_delimiter;

    ptrdiff_t len = client->static_response_body.len +
                    client->dynamic_response_body.len - 1;
    Str content_length = STR(TO_STRING(len));
    (void)len;

    printf("len: %.*s\n", (int)content_length.len, content_length.data);

    Str *static_array[] = {&client->http_ver,
                           &SPACE,
                           &client->response_status,
                           &LINEBREAK,
                           &LINEBREAK,
                           &before_delimiter,
                           &client->dynamic_response_body,
                           &after_delimiter,
                           &TRAILER};
    for (u_long i = 0; i < (sizeof static_array / sizeof(Str *)); i++)
      if (write_str(client, static_array[i]) < 0)
        return err("Writing response", true);
  } else {
    Str *dynamic_array[] = {
        &client->http_ver, &SPACE,     &client->response_status,
        &LINEBREAK,        &LINEBREAK, &client->dynamic_response_body,
        &TRAILER};
    for (u_long i = 0; i < (sizeof dynamic_array / sizeof(Str *)); i++)
      if (write_str(client, dynamic_array[i]) < 0)
        return err("Writing response", true);
  }

  return 0;
}

int write_str(Client *client, Str *str) {
  if (!client || !str)
    return null_ptr("Null Str pointer");

  ptrdiff_t current = 0;

  while (current < str->len) {
    long wrote =
        write(client->fd, str->data + current, (size_t)(str->len - current));
    if (wrote < 0)
      return err("Writing response Str", true);
    current += wrote;
  }

  return 0;
}

int generate_response(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // Metadata of the dir/file
  struct stat s;

  // Although the file is already checked to exist, just making sure
  // The path is null termianted from request.c
  if (stat(client->request_path.data, &s) == -1)
    return err("Invalid path", true);
  else if (S_ISREG(s.st_mode)) // File
    return read_dynamic_file(client);
  else if (S_ISDIR(s.st_mode)) // Directory
    return read_directory(client);
  else {
    errno = EIO;
    return err("Accessing file/directory", true);
  }

  return 0;
}

// Deals with every user requested file
int read_dynamic_file(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  Str *body = &client->dynamic_response_body;

  FILE *file = fopen(client->request_path.data, "r");

  if (!file)
    return err("Opening file", true);

  if (fseek(file, 0, SEEK_END) != 0)
    return err("Seeking to the end", true);
  body->len = ftell(file);

  rewind(file);

  if (!(body->data = (char *)malloc((u_long)body->len))) {
    body->len = 0;
    return err("Malloc dynamic response", true);
  }

  if (fread(body->data, 1, (u_long)body->len, file) != (u_long)body->len)
    return err("Reading request file", true);

  fclose(file);

  return 0;
}

// Deals with every user requested directory and calls read_static_file cause
// contents of SERVER_HTML are required to render the directory contents
int read_directory(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  struct dirent *dir_entry;
  DIR *dir;
  Str *body = &client->dynamic_response_body;

  // First calculating the final size, I don't prefer fixed length buffers
  // Again, the request path is null terminated
  if ((dir = opendir(client->request_path.data))) {
    while ((dir_entry = readdir(dir))) {
      // skipping current and previous dir entries
      if (memcmp(dir_entry->d_name, "..", 2) != 0 &&
          memcmp(dir_entry->d_name, ".", 1) != 0)
        body->len += (ptrdiff_t)strlen(dir_entry->d_name) +
                     1; // Adding one cause i need \n
    }

    if (!(body->data = (char *)malloc((size_t)body->len))) {
      closedir(dir);
      return err("Malloc dynamic response", true);
    }

    rewinddir(dir);

    // Actually reading dirs
    while ((dir_entry = readdir(dir))) {
      u_long pos = 0;
      size_t dir_len = strlen(dir_entry->d_name);
      // skipping current and previous dir entries
      if (memcmp(dir_entry->d_name, "..", 2) != 0 &&
          memcmp(dir_entry->d_name, ".", 1) != 0 &&
          pos + dir_len < (u_long)body->len) {
        memcpy(body->data + pos, dir_entry->d_name, dir_len);
        memcpy(body->data + pos + dir_len, "\n", 1);
        pos += dir_len;
      }
    }

    closedir(dir);
  } else
    return err("Opening directory", true);

  for (size_t i = 0; i < STATIC_COUNT; ++i)
    if (strcmp(STATIC_FILES[i], SERVER_HTML) == 0)
      if (read_static_file(client, STATIC_PATHS[i].data) < 0 ||
          find_delimiter(client) < 0)
        return err("Handling static file", false);

  return 0;
}

// Only to be called by read_directory and for now is just meant to read
// SERVER_HTML, but could read more files from the STATC_DIR if needed
// Reads into the static_response_body
int read_static_file(Client *client, const char *filepath) {
  if (!client || !filepath)
    return null_ptr("Invalid client or file pointer");

  Str *body = &client->static_response_body;

  FILE *file = fopen(filepath, "r");

  if (!file)
    return err("Opening file", true);

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return err("Seeking to the end", true);
  }
  body->len = ftell(file);

  rewind(file);

  if (!(body->data = (char *)malloc((u_long)body->len))) {
    body->len = 0;
    fclose(file);
    return err("Malloc static response", true);
  }

  if (fread(body->data, 1, (u_long)body->len, file) != (u_long)body->len) {
    fclose(file);
    return err("Reading request file", true);
  }

  fclose(file);

  return 0;
}

void print_response(Client *client) {

  // Str response_array[7] = {
  //     client->http_ver, SPACE,     client->response_status,
  //     LINEBREAK,        LINEBREAK, client->dynamic_response_body,
  //     TRAILER};
  Str *response_array[] = {&client->dynamic_response_body, &TRAILER};

  for (u_long i = 0; i < 2; i++)
    printf("%.*s", (int)response_array[i]->len, response_array[i]->data);
  return;
}

int find_delimiter(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  for (ptrdiff_t i = 0; i < client->static_response_body.len; i++)
    if (memcmp(client->static_response_body.data + i, HTTP_DELIMITER, 1) == 0) {
      client->static_delimiter = i;
      return 0;
    }
  return err("Delimiter not found", false);
}
