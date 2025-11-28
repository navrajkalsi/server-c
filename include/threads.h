#pragma once

#include <stdbool.h>

// creates threads in the thread pool
bool create_threads(void);

// thread function that check for new work for a thread has to return and accept void*
void *handle_thread(void *arg);

// checks if server is stopped and then broadcasts the cond var
// since the condition for while loop of handle_thread() is to check RUNNING
// the while loop exits and the threads will not be busy
// Then joins the threads back
void cleanup_pool(void);
