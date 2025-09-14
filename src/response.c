#include <dirent.h>
#include <errno.h>
#include <magic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "args.h"
#include "client.h"
#include "main.h"
#include "request.h"
#include "response.h"
#include "utils.h"

bool handle_response(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // If response code not set, something is not right
  client->response_status =
      ASSIGN_IF_NULL(client->response_status, "500 Internal Server Error");

  // If response is not OK, then print the error message on client side
  // now need to generate response
  if (equals(&client->response_status, &STR("200 OK")) &&
      !generate_response(client)) {
    client->response_status = STR("500 Internal Server Error");
    err("Generating response", true);
  }

  if (!equals(&client->response_status, &STR("200 OK"))) {
    free_client_members(client); // Freeing any previous response bodies and
                                 // content_type As length will be set later
    generate_error(client);
  }

  if (!set_content_length(client))
    err("Setting content length", false);

  if (!set_date(client))
    err("Setting date", false);

  if (config.debug)
    print_client(client);

  if (!write_response(client)) // No hope now, have to return :)
    return err("Writing response", true);

  return true;
}

bool write_response(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  if (!write_headers(client))
    return err("Writing headers", true);

  if (!write_response_body(client))
    return err("Writing body", true);

  return print_debug("Wrote full response to the client FD");
}

bool write_str(const Client *client, const Str *str) {
  if (!client || !str)
    return null_ptr("Null Str pointer");

  ptrdiff_t current = 0;

  while (str->len && current < str->len) {
    long wrote =
        write(client->fd, str->data + current, (size_t)(str->len - current));
    if (wrote < 0)
      return err("Writing response Str", true);
    current += wrote;
  }

  return true;
}

bool write_headers(const Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  const Str *headers[] = {
      &client->http_ver,
      &SPACE,
      &client->response_status,
      &STR("\r\nContent-Type: "),
      &client->content_type,
      &STR("\r\nContent-Length: "),
      &client->content_length,
      &STR("\r\nConnection: "),
      &client->connection,
      &STR("\r\nDate: "),
      &client->date,
      &STR("\r\nAccess-Control-Allow-Origin: "
           "*\r\nAccess-Control-Expose-Headers: Content-Type\r\n\r\n"),
      // Exposing Content-Type to make previewing easier in JS later
  };

  for (u_long i = 0; i < (sizeof headers / sizeof(Str *)); i++)
    if (!write_str(client, headers[i]))
      return err("Writing response headers", true);

  return print_debug("Wrote all headers to the client FD");
}

bool write_response_body(const Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // Serving SERVER_HTML with file listings
  // or ERROR_HTML with response status
  if (client->static_response_body.len && client->static_delimiter) {
    Str before_delimiter, after_delimiter;
    before_delimiter = after_delimiter = client->static_response_body;

    before_delimiter.len = client->static_delimiter;
    after_delimiter.data += client->static_delimiter + 1;
    after_delimiter.len -= client->static_delimiter + 1;

    const Str *static_array[] = {&before_delimiter,
                                 &client->dynamic_response_body,
                                 &after_delimiter, &TRAILER};

    for (u_long i = 0; i < (sizeof static_array / sizeof(Str *)); i++)
      if (!write_str(client, static_array[i]))
        return err("Writing static response body", true);
  } else {
    const Str *dynamic_array[] = {&client->dynamic_response_body, &TRAILER};

    for (u_long i = 0; i < (sizeof dynamic_array / sizeof(Str *)); i++)
      if (!write_str(client, dynamic_array[i]))
        return err("Writing dynamic response body", true);
  }
  return print_debug("Wrote full response body to the client FD");
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
}

bool generate_error(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // Serving a simple error code to the client
  client->dynamic_response_body.len = client->response_status.len;
  client->dynamic_response_body.data = strdup(client->response_status.data);

  return read_static_file(client, STATIC_PATHS[ERR].data) &&
         find_delimiter(client);
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

  if (body->len && !(body->data = (char *)malloc((u_long)body->len))) {
    body->len = 0;
    return err("Malloc dynamic response", true);
  }

  if (fread(body->data, 1, (u_long)body->len, file) != (u_long)body->len)
    return err("Reading request file", true);

  fclose(file);

  // Libmagic sets MIME of .js files to text/plain
  // In order for scripts to work the mime should be application/javascript
  // Dealing with _server.js only here
  if (equals(&client->request_path, &STATIC_PATHS[JS]))
    client->content_type = str_data_malloc("application/javascript");
  else {
    if (!set_content_type(client, NULL))
      return err("Setting content type", true);
  }

  return print_debug("Read dynamic file (user requested) to the buffer");
}

// Deals with every user requested directory and calls read_static_file cause
// contents of SERVER_HTML are required to render the directory contents
bool read_directory(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  struct dirent *dir_entry;
  DIR *dir;
  Str *body = &client->dynamic_response_body;
  StrList dir_list = {.head = NULL, .tail = NULL};

  // First calculating the final size, I don't prefer fixed length buffers
  // Again, the request path is null terminated
  if ((dir = opendir(client->request_path.data))) {
    while ((dir_entry = readdir(dir))) {
      // skipping current and previous dir entries
      if (!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, ".."))
        continue;

      Str *dir_str = str_malloc(dir_entry->d_name);
      if (!dir_str) {
        list_free(&dir_list);
        closedir(dir);
        return err("Calculating body length", true);
      }

      if (dir_entry->d_type == DT_DIR)
        dir_str->data[dir_str->len++] =
            '/'; // replacing null terminator to / in case of dir, to
                 // distinguish between dirs & files in the frontend

      body->len += dir_str->len + 1; // incrementing to accomodate \n delimiter

      StrNode *dir_node = str_node_malloc(dir_str);
      if (!dir_node) {
        str_free(&dir_str);
        list_free(&dir_list);
        closedir(dir);
        return err("Calculating body length", true);
      }

      list_append(&dir_list, dir_node);
    }
    closedir(dir);
  } else
    return err("Opening directory", true);

  if (body->len && dir_list.tail &&
      !(body->data = (char *)malloc((size_t)body->len))) {
    body->len = 0;
    list_free(&dir_list);
    return err("Malloc dynamic response", true);
  }

  // writing entries to the body
  StrNode *current = dir_list.head;
  StrNode *next = NULL;
  u_long pos = 0;

  while (current) {
    next = current->next;
    if (pos + current->str->len + 1 > (u_long)body->len)
      break;
    memcpy(body->data + pos, current->str->data, current->str->len);
    pos += current->str->len;
    body->data[pos++] = '\n';

    node_free(current);
    current = next;
  }

  if (!read_static_file(client, STATIC_PATHS[HTML].data) ||
      !find_delimiter(client))
    return err("Handling static file", false);

  return print_debug("Read user request directory contents into the buffer");
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

  if (body->len && !(body->data = (char *)malloc((u_long)body->len))) {
    body->len = 0;
    fclose(file);
    return err("Malloc static response", true);
  }

  if (fread(body->data, 1, (u_long)body->len, file) != (u_long)body->len) {
    fclose(file);
    return err("Reading request file", true);
  }

  fclose(file);

  if (!set_content_type(client, filepath))
    return err("Setting content type", true);

  return print_debug("Read static file (not directly user requested, usually) "
                     "into the buffer");
}

void print_response(const Str *response_array[], int array_len) {
  for (int i = 0; i < array_len; i++)
    str_print(response_array[i]);
}

bool find_delimiter(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  for (ptrdiff_t i = 0; i < client->static_response_body.len; i++)
    if (memcmp(client->static_response_body.data + i, HTTP_DELIMITER, 1) == 0) {
      client->static_delimiter = i;
      return print_debug("Delimiter found");
    }
  return err("Delimiter not found", false);
}

bool set_content_type(Client *client, const char *path) {
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

  // copied the mime before closing
  magic_close(magic);

  if (mime) {
    client->content_type.data = mime;
    client->content_type.len = (ptrdiff_t)strlen(mime);
    return print_debug("Content type set");
  } else
    return err("Magic file", false);
}

bool set_content_length(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  ptrdiff_t final_len = client->static_response_body.len > 0
                            ? client->static_response_body.len +
                                  client->dynamic_response_body.len - 1
                            : client->dynamic_response_body.len;

  // the str is freed in free_client
  if (!int_to_string((int)final_len, &client->content_length))
    return err("Converting length to string", false);

  return print_debug("Content length set");
}

bool set_date(Client *client) {
  if (!client)
    return null_ptr("Invalid client pointer");

  // HTTP date format is always the same len
  if (!(client->date.data = (char *)malloc((size_t)DATE_LEN)))
    return err("Malloc date error", true);
  client->date.len =
      DATE_LEN -
      1; // subtracting one because the last char would be a null terminator

  time_t now = time(NULL);
  struct tm tm;
  gmtime_r(&now, &tm);

  // strftime returns 0 if write buffer is small
  return (bool)strftime(client->date.data, (size_t)DATE_LEN,
                        "%a, %d %b %Y %H:%M:%S GMT", &tm) &&
         print_debug("Date header set");
}
