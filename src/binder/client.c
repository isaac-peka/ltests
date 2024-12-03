#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "ubinder.h"
#include "logger.h"
#include "binder.h"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

volatile int ready = 0;

void * thread_worker(void *arg) {
  pthread_mutex_lock(&lock);

  while (!ready) 
    pthread_cond_wait(&cond, &lock);

  pthread_mutex_unlock(&lock);
}

int setup() {
  int status = 0;
  ubinder_state state;
  ubinder_thread_state *tstate;

  LOG(LOG_PROGRESS, "main: Initializing ubinder_state");
  status = ubinder_init(&state);
  if (status == -1) {
    goto error;
  }

  status = ubinder_thread_init(&tstate);
  if (status == -1) {
    LOGFMT(LOG_BAD, "main: Failed to set up thread state: %s", strerror(errno));
    goto error;
  }

error:
  LOGKICK();
  return status;
}

int do_transaction(ubinder_state *state, ubinder_thread_state *tstate) {
  struct binder_transaction_data tr = {0};
  struct flat_binder_object objs[1];
  
  tr.code = 1;
  tr.data_size = 0;
  tr.offsets_size = 0;
}

int main() {
  int status;
  pthread_t worker;

  status = logger_init();
  if (status == -1) {
    printf("[-] main: FATAL: Could not initialize logger\n");
    exit(1);
  }

  LOG(LOG_PROGRESS, "main: Initializing worker");
  status = pthread_create(&worker, NULL, thread_worker, NULL);
  if (status == -1) {
    LOGFMT(LOG_BAD, "main: Failed to create thread: %s", strerror(errno));
    status = 1;
    goto done;
  }

  status = setup();
  if (status == -1) {
    status = 1;
    goto done;
  }

done:
  logger_stop();
  return status;
}