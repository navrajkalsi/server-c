#include "../include/threads.h"
#include "../include/main.h"
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

bool create_threads(void) {
  if (THREAD_POOL_SIZE < 1)
    return err("Thread pool insufficient", false);

  for (int i = 0; i < THREAD_POOL_SIZE; i++)
    if ((errno = pthread_create(&thread_pool[i], NULL, handle_thread, NULL)))
      return err("Creating thread", true);

  return true;
}

void *handle_thread(void *arg) {
  (void)arg;
  while (RUNNING) {
    Client *client;
    // mutex lock ensures that only one of the threads tries to connect and
    // handle a client
    pthread_mutex_lock(&mutex);
    // this thread now waits till it is signalled
    // this also releases the mutex lock so that other threads can access the
    // clients list
    // when signalled, it acquires the lock again to continue handling
    // connection
    while (!(client = dequeue_client()) && RUNNING)
      // waiting only if there is no new work
      pthread_cond_wait(&condition_var, &mutex);

    pthread_mutex_unlock(&mutex);

    if (!client)
      continue;

    handle_client(client);
    close(client->fd);
    free_client(&client);
  }

  return NULL;
}
