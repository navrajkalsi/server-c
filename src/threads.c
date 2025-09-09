#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "../include/args.h"
#include "../include/client.h"
#include "../include/main.h"
#include "../include/threads.h"
#include "../include/utils.h"

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
  if (config.debug)
    printf("Currently in thread: %lu\n", (unsigned long)pthread_self());

  while (RUNNING) {
    Client *client = NULL;
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

  return (void *)print_debug("Thread handled");
}

void cleanup_pool(void) {
  if (RUNNING)
    return (void)err("Server not stopped", false);

  // signalling threads to exits the loop
  pthread_cond_broadcast(&condition_var);

  // pthread_join waits for each thread to terminate
  for (int i = 0; i < THREAD_POOL_SIZE; i++)
    if ((errno = pthread_join(thread_pool[i], NULL)))
      return (void)err("Joining thread", true);

  // remove all clients
  pthread_mutex_lock(&mutex);
  Client *client;
  while ((client = dequeue_client())) {
    close(client->fd);
    free_client(&client);
  }
  pthread_mutex_unlock(&mutex);

  return (void)print_debug("Cleaned up the server pool & closed all clients");
}
