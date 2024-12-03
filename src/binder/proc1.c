#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

#include "ubinder.h"
#include "logger.h"

const char * const SOCKPATH = "/dev/proc2.sock";

int send_binder(int fd, int binderfd) {
  struct msghdr msg = {0};
  struct cmsghdr * cmsg;
  struct iovec iov;
  char buf[256];

  iov.iov_base = "A";
  iov.iov_len = 1;

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_controllen =  CMSG_SPACE(sizeof(int));
  msg.msg_control = buf;

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));

  *((int *) CMSG_DATA(cmsg)) = binderfd;

  return sendmsg(fd, &msg, 0);
}

int open_binder(ubinder_state *state, ubinder_thread_state **tstate) {
  if (ubinder_init(state) == -1) {
    LOG(LOG_BAD, "[-] Failed to initialize ubinder\n");
    return -1;
  }

  if (ubinder_thread_init(tstate) == -1) {
    LOG(LOG_BAD, "[-] Failed to initialize thread\n");
    goto tstate_err;
  }

  return 0;
tstate_err: 
  ubinder_free(state);
  return -1;
}

int main(int argc, char **argv) {
  ubinder_state state;
  ubinder_thread_state *tstate;
  struct sockaddr_un addr = {0};
  int sfd;
  int cfd;
  int binderfd;

  if (logger_init() == -1) {
    printf("Failed to initialize logger\n");
    exit(1);
  }

  binderfd = open_binder(&state, &tstate);
  if (binderfd == -1) {
    LOGFMT(LOG_BAD, "Failed to open binder\n");
    goto error;
  }
  
  sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sfd == -1) {
    LOGFMT(LOG_BAD, "Failed to create socket: %s\n", strerror(errno));
    goto error;
  }

  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, SOCKPATH);

  if (connect(sfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    LOGFMT(LOG_BAD, "Failed to connect: %s\n", strerror(errno));
    goto error;
  }

  if (send_binder(sfd, state.fd) == -1) {
    LOGFMT(LOG_BAD, "Failed to send binder fd: %s\n", strerror(errno));
    goto error;
  }

  LOG(LOG_OK, "Sent binder fd");
  close(state.fd);
  LOG(LOG_OK, "Closed binder fd");
  logger_stop();
  return 0;
error:
  logger_stop();
  return 1;
}
