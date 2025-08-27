#include "../include/response.h"
#include "../include/request.h"
#include <dirent.h>
#include <errno.h>
#include <magic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Response bodies are the only malloced vars in this whole program
// response_mime_type is also to be freed

bool handle_response(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  print_client(client);
  // Setting both response and mime structs to null
  client->dynamic_response_body = client->static_response_body =
      client->response_mime = ERR_STR;

  // If response code not set, something is not right
  if (!client->response_status.data || !client->response_status.len)
    client->response_status = STR("500 Internal Server Error");

  // If response is not OK, then print the error message on client side
  if (equals(&client->response_status, &STR("200 OK")))
    if (!generate_response(client))
      return err("Generating response", true);

  if (!write_response(client))
    return err("Writing response", true);

  return true;
}

bool write_response(Client *client) {
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

    Str *static_array[] = {
        &client->http_ver,
        &SPACE,
        &client->response_status,
        &STR("\r\nContent-Type: "),
        &client->response_mime,
        &STR("\r\nConnection: Close\r\nAccess-Control-Allow-Origin: "
             "*\r\nAccess-Control-Expose-Headers: Content-Type\r\n\r\n"),
        &before_delimiter,
        &client->dynamic_response_body,
        &after_delimiter,
        &TRAILER};

    for (u_long i = 0; i < (sizeof static_array / sizeof(Str *)); i++)
      if (!write_str(client, static_array[i]))
        return err("Writing response", true);
  } else {
    Str *dynamic_array[] = {
        &client->http_ver,
        &SPACE,
        &client->response_status,
        &STR("\r\nContent-Type: "),
        &client->response_mime,
        &STR("\r\nConnection: Close\r\nAccess-Control-Allow-Origin: "
             "*\r\nAccess-Control-Expose-Headers: Content-Type\r\n\r\n"),
        &client->dynamic_response_body,
        &TRAILER};

    for (u_long i = 0; i < (sizeof dynamic_array / sizeof(Str *)); i++)
      if (!write_str(client, dynamic_array[i]))
        return err("Writing response", true);
  }

  return true;
}

bool write_str(Client *client, Str *str) {
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

  return true;
}

bool generate_response(Client *client) {
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

  return true;
}

// Deals with every user requested file
bool read_dynamic_file(Client *client) {
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

  // Libmagic sets MIME of .js files to text/plain
  // In order for scripts to work the mime should be application/javascript
  // Dealing with _server.js only here
  if (equals(&client->request_path, &STATIC_PATHS[JS])) {
    client->response_mime.data = strdup("application/javascript");
    client->response_mime.len = (ptrdiff_t)strlen(client->response_mime.data);
  } else {
    if (!get_mime_type(client, NULL))
      return err("Getting MIME", true);
  }

  return true;
}

// Deals with every user requested directory and calls read_static_file cause
// contents of SERVER_HTML are required to render the directory contents
bool read_directory(Client *client) {
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

    u_long pos = 0;
    // Actually reading dirs
    while ((dir_entry = readdir(dir))) {
      size_t dir_len = strlen(dir_entry->d_name);
      // skipping current and previous dir entries
      puts(dir_entry->d_name);
      if (memcmp(dir_entry->d_name, "..", 2) != 0 &&
          memcmp(dir_entry->d_name, ".", 1) != 0 &&
          pos + dir_len < (u_long)body->len) {
        memcpy(body->data + pos, dir_entry->d_name, dir_len);
        memcpy(body->data + pos + dir_len, "\n", 1);
        pos += dir_len + 1;
      }
    }
    closedir(dir);
  } else
    return err("Opening directory", true);

  if (!read_static_file(client, STATIC_PATHS[HTML].data) ||
      !find_delimiter(client))
    return err("Handling static file", false);

  printf("dy: %.*s\n", (int)body->len, body->data);
  return true;
}

// Only to be called by read_directory and for now is just meant to read
// SERVER_HTML, but could read more files from the STATC_DIR if needed
// Reads into the static_response_body
bool read_static_file(Client *client, const char *filepath) {
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

  if (!get_mime_type(client, filepath))
    return err("Getting MIME", true);

  return true;
}

void print_response(Str *response_array[], int array_len) {
  for (int i = 0; i < array_len; i++)
    printf("%.*s\n", (int)response_array[i]->len, response_array[i]->data);
}

bool find_delimiter(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  for (ptrdiff_t i = 0; i < client->static_response_body.len; i++)
    if (memcmp(client->static_response_body.data + i, HTTP_DELIMITER, 1) == 0) {
      client->static_delimiter = i;
      return true;
    }
  return err("Delimiter not found", false);
}

bool get_mime_type(Client *client, const char *path) {
  if (!client)
    return null_ptr("Invalid client pointer");

  magic_t magic;

  if (!(magic = magic_open(MAGIC_MIME_TYPE)))
    return err("Magic open", true);

  if (magic_load(magic, NULL) != 0) {
    magic_close(magic);
    return err("Magic load", false);
  }

  char *mime;
  if (path)
    mime = strdup(magic_file(magic, path));
  else
    mime = strdup(magic_file(magic, client->request_path.data));

  magic_close(magic);

  if (mime) {
    client->response_mime.data = mime;
    client->response_mime.len = (ptrdiff_t)strlen(mime);
    return true;
  } else
    return err("Magic file", false);
}
