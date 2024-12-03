#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <pthread.h>

#include "logger.h"

static TAILQ_HEAD(log_head, log_entry) head;

static struct log_head head;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_t worker;

static volatile bool setup = false;
static volatile bool dead = false;

static void logger_timedwait() {
  struct timespec target;

  clock_gettime(CLOCK_REALTIME, &target);
  target.tv_sec += 5;
  pthread_cond_timedwait(&cond, &lock, &target);
}

static void logger_consume() {
  while (true) {
    const char *status;
    struct log_entry *entry;

    entry = TAILQ_FIRST(&head);
  
    if (entry == NULL)
      return;
  
    switch(entry->status) {
      case LOG_OK:
        status = "[+]";
        break;
      case LOG_BAD:
        status = "[-]";
        break;
      case LOG_PROGRESS:
        status = "[/]";
        break;
      default:
        __builtin_unreachable();
    }

    printf("%s %s\n", status, entry->msg);
    TAILQ_REMOVE(&head, entry, entries);
    free(entry->msg);
    free(entry);
  }
}

static void * logger_worker(void *arg) {
  pthread_mutex_lock(&lock);

  while (!dead) {
    logger_timedwait();
    logger_consume();
  }

  logger_consume();
  pthread_mutex_unlock(&lock);
}

int logger_init() {
  int status;
  TAILQ_INIT(&head);
  dead = false;
  status = pthread_create(&worker, NULL, logger_worker, NULL);
  return status;
}

void logger_kick() {
  pthread_mutex_lock(&lock);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&lock);
}

void logger_stop() {
  dead = true;
  pthread_mutex_lock(&lock);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&lock);
  pthread_join(worker, NULL);
}

void logger_enqueue(log_status status, const char * line) {
  struct log_entry *entry;

  entry = malloc(sizeof(struct log_entry));
  assert(entry != NULL);

  entry->status = status;
  entry->msg = strdup(line);
  assert(entry->msg != NULL);

  pthread_mutex_lock(&lock);
  TAILQ_INSERT_TAIL(&head, entry, entries);
  pthread_mutex_unlock(&lock);
}

void logger_enqueue_format(log_status status, const char *fmt, ...) {
  va_list va;
  size_t len = 0;
  char *line = NULL;

  va_start(va, fmt);
  len = vsnprintf(line, len, fmt, va);
  va_end(va);
  assert(len >= 0);

  len = len + 1;
  line = malloc(len);
  assert(line != NULL);

  va_start(va, fmt);
  len = vsnprintf(line, len, fmt, va);
  va_end(va);
  assert(len >= 0);

  logger_enqueue(status, line);
}
