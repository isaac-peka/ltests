#pragma once

#include <sys/queue.h>

typedef enum log_status {
  LOG_OK,
  LOG_BAD,
  LOG_PROGRESS,
} log_status;

struct log_entry {
  log_status status;
  char *msg;
  TAILQ_ENTRY(log_entry) entries;
};

int logger_init();
void logger_start();
void logger_kick();
void logger_stop();
void logger_enqueue(log_status status, const char * line);
void logger_enqueue_format(log_status status, const char *line, ...);

#define LOG logger_enqueue
#define LOGFMT logger_enqueue_format
#define LOGKICK logger_kick
