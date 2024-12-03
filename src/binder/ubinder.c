#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "binder.h"
#include "ubinder.h"
#include "logger.h"


#define BINDER_VM_MMAP_SIZE 0x10000
#define BINDER_DEVICE "/dev/binder"
#define BINDER_THREAD_READ_BUFFER_SZ 0x1000

#define MREMAP_FIXED 2

int ubinder_init(ubinder_state *state) {
  int fd;
  void *vm;
  void *vm2;
  void *vm3;

  fd = open(BINDER_DEVICE, O_RDWR);
  if (fd == -1) {
    LOGFMT(LOG_BAD, "Failed to open /dev/binder: %s", strerror(errno));
    goto error;
  } 

  vm = mmap(NULL, BINDER_VM_MMAP_SIZE, PROT_READ, 
              MAP_PRIVATE | MAP_NORESERVE, fd, 0);
  if (vm == NULL) {
    LOGFMT(LOG_BAD, "Failed to mmap /dev/binder: %s", strerror(errno));
    goto error;
  }

  LOGFMT(LOG_OK, "mmap address=%p", vm);

  state->fd = fd;
  state->vm_size = BINDER_VM_MMAP_SIZE;
  state->vm_start = vm;

  return 0;
error:
  LOGKICK();
  return -1;
}

void ubinder_free(ubinder_state *state) {
  close(state->fd);
}

int ubinder_thread_init(ubinder_thread_state **out) {
  ubinder_thread_state *state = malloc(sizeof(ubinder_thread_state));
  if (state == NULL)
    return -1;

  state->read_buffer_size = BINDER_THREAD_READ_BUFFER_SZ;
  state->read_buffer = malloc(state->read_buffer_size);

  if (!state->read_buffer) {
    free(state);
    return -1;
  }

  *out = state;
  return 0;
}

void ubinder_thread_free(ubinder_thread_state *state) {
  free(state->read_buffer);
  free(state);
}

int ubinder_become_context_mgr(ubinder_state *state) {
  int status;

  struct flat_binder_object obj = {
    .flags = FLAT_BINDER_FLAG_TXN_SECURITY_CTX
  };

  status = ioctl(state->fd, BINDER_SET_CONTEXT_MGR_EXT, &obj);
  return status;
}

int ubinder_write_read(ubinder_state *state, ubinder_thread_state *tstate, 
    void *buffer, size_t len) {

  int status;
  struct binder_write_read obj;

  obj.write_size = len;
  obj.write_consumed = 0;
  obj.write_buffer = (binder_uintptr_t) buffer;
  obj.read_size = tstate->read_buffer_size;
  obj.read_consumed = 0;
  obj.read_buffer = (uint64_t) tstate->read_buffer;

  status = ioctl(state->fd, BINDER_WRITE_READ, &obj);
  return status;
}

int ubinder_enter_looper(ubinder_state *state, ubinder_thread_state *tstate) {
  int status;
  struct binder_write_read obj;
  uint64_t cmd = BC_ENTER_LOOPER;
  return ubinder_write_read(state, tstate, &cmd, sizeof(cmd));
}

int ubinder_register_looper(ubinder_state *state, ubinder_thread_state *tstate) {
  return 0;
}

int ubinder_send_transaction(ubinder_state *state, ubinder_thread_state *tstate, 
  ubinder_transaction *transaction) {

  // int i;
  // struct binder_transaction_data tr = {0};
  // void *buf;
  // binder_size_t *offsets;
  // size_t off = 0;

  // tr.code = transaction->code;
  // tr.target.handle = transaction->handle;
  // tr.flags = transaction->flags;
  // tr.offsets_size = transaction->objcount * sizeof(binder_size_t);

  // // Calculate the size of the buffer and offsets
  // for (i = 0; i < transaction->objcount; i++) {
  //   ubinder_object *obj = transaction->objs[i];
  //   tr.data_size += obj->len;

  //   if (obj->type == UBINDER_BINDER_OBJECT) {
  //     tr.offsets_size += sizeof(binder_size_t);
  //   }
  // }

  // buf = malloc(tr.offsets_size + tr.data_size);
  // offsets = (binder_size_t *) buf;

  // // Fixup buffer addresses
  // for (i = 0; i < transaction->objcount; i++) {
  //   ubinder_object *obj = transaction->objs[i];

  //   if (obj->type == UBINDER_BLOB) {
  //     memcpy(buf + off, obj->blob, obj->len);
  //     off += obj->len;
  //     continue;
  //   }

  //   switch (obj->object->type) {
  //     case BINDER_TYPE_PTR: {
  //       struct binder_buffer_object *buf = (struct binder_buffer_object *) obj->object;
  //       assert(obj->len >= sizeof(*buf));
  //       break;
  //     }
  //     case BINDER_TYPE_FDA: {
  //       struct binder_fd_array_object *fda = (struct binder_fd_array_object *) obj->object;
  //       assert(obj->len >= sizeof(*fda));
  //       break;
  //     }
  //   }

  //   memcpy(buf + off, obj->object, obj->len);
  //   *offsets++ = off;
  //   off += obj->len;
  // }

  
  return 0;
}