#include "main.h"
#include "response.h"
#include <pthread.h>

// creates threads in the thread pool
bool create_threads(void);

// thread function that check for new work for a thread
// has to return and accept void*
void *handle_thread(void *arg);
