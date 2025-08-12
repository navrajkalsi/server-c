#pragma once

// OS switch
// #ifdef does not seem to work with complex conditions
#if defined(WIN32) || defined(_WIN32)
#define OS_WIN
#elif defined(UNIX) || defined(__unix__) || defined(LINUX) || defined(__linux__)
#define OS_UNIX
#else
#error Operation System not supported.
#endif

// Args.c specific
#define DEFAULT_PORT 1419
#define DEFAULT_ROOT_DIR "./"
