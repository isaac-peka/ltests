#define _GNU_SOURCE

#include <unistd.h>
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

  ubinder_become_context_mgr(&state);
  if (status == -1) {
    LOGFMT(LOG_BAD, "main: Failed to become context mgr: %s", strerror(errno));
    goto error;
  }

  // TEST

  void *vm2;
  vm2 = mremap(state.vm_start, state.vm_size, BINDER_VM_MMAP_SIZE, MREMAP_MAYMOVE | MREMAP_DONTUNMAP, 0xd3ad0000);
  LOGFMT(LOG_OK, "mremap address=%p", vm2);

  if (vm2 == MAP_FAILED) {
    LOGFMT(LOG_BAD, "Failed to remap: %s", strerror(errno));
    goto error;
  }

  LOGFMT(LOG_PROGRESS, "Calling munmap", strerror(errno));
  munmap(state.vm_start, state.vm_size);

  LOGFMT(LOG_PROGRESS, "Calling close", strerror(errno));
  close(state.fd);

  LOGFMT(LOG_OK, "Finished - exiting", strerror(errno));
error:
  LOGKICK();
  return status;
}

int main() {
  int status;
  pthread_t worker;
  void *vm2;

  status = logger_init();
  if (status == -1) {
    printf("[-] main: FATAL: Could not initialize logger");
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