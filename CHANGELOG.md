# Changelog

## [1.0]
### Added
- Initial release.
- Basic HTTP server written in C with support for:
  - `IPv4` connections.
  - Handling multiple clients, using `fork()`.
  - Graceful shutdown on `SIGINT` / `SIGTERM`.
  - Reaping child processes via `SIGCHLD` handler.
  - Request parsing.
  - `Directory listing`, incase the request path is a directory.
  - Serving static helper files from configurable directory.
  - MIME type detection & content-type header, with `libmagic`.
  - Root `directory traversal`.
  - File `previews` (text, image, pdf).
  - Basic `vim motions` to navigate.
- Configurable options via command-line flags:
  - `-a` → accept connections from **all network interfaces** (default: loopback only).
  - `-d` → debug mode (prints function calls).
  - `-p` → port (default: `1419`).
  - `-r` → set root directory to serve.

### Notes
- Currently only supports `HTTP/1.1` with `Connection: close`.
- Currently only supports `GET` requests.
- `404` is the only error served.
- Limited `HTTP response codes`.

## [2.0]
### Changed
- Major refactor of the server into modular components:
  - **args.c** → configs the server with flags used.
  - **server.c** → socket setup, signal handling, socket timeouts, accept loop.
  - **client.c** → client connection handling with keep-alive loop, reading client socket.
  - **request.c** → HTTP request parsing and validation.
  - **response.c** → HTTP response generation and error handling.
  - **threads.c** → thread managements.
  - **utils.c** → shared helper functions.
- More `robust` path traversal check.
- Forking is deprecated in favour of `threading`.
- IPv4 addresses are now mapped as IPv6.
- Client is handled outside of server listening loop.
- The server process' directory changes to the root_dir, with `chdir()`.
- Response header are now `HTTP version-agnostic`.

### Added
- Threading (`threads.c`) for better performance & scalability.
- Utility module (`utils.c`) for reusable helpers.
- `Client queue`, linked list to be used by threads.
- Proper `HTTP response status` codes.
- `keep-alive` support, depending on the `connection` request header.
- `Error reporting` to the client.
- `Date` header to the response headers.
- Respecting client's HTTP version.
- `IPv6` connection support.
- Custom error page with styles.

### Deprecated
- Forking.

## [2.1]
### Changed
- Resolved file listing bug for `files starting with a period`.
- Size of dir contents is calculated and the entries are now managed by a `linked list`.

### Added
- `SIGPIPE` signal handling.
- Support for `% encoded ASCII` chars in the URL.
- + can also be used for a space in the URL.
- `URL parameter` to change the directory and index.html file serving behaviour.

## [2.2]
### Changed
- Made `changing libmagic MIMES` more robust & dynamic.

### Added
- Custom MIMES are handled dynamically with new arrays of file extensions and replacement MIMES.
