#pragma once

#include "binder.h"

#define BINDER_VM_MMAP_SIZE 0x10000
#define BINDER_DEVICE "/dev/binder"
#define BINDER_THREAD_READ_BUFFER_SZ 0x1000

typedef struct ubinder_state {
  int fd;
  void * vm_start;
  size_t vm_size;
} ubinder_state;

typedef struct ubinder_thread_state {
  void *read_buffer;
  binder_size_t read_buffer_size;
} ubinder_thread_state;

typedef enum ubinder_object_type {
  UBINDER_BINDER_OBJECT,
  UBINDER_BLOB
} ubinder_object_type;


/**
 * @brief Unified type for binder objects and blobs copied straight into the
 * buffer. Note that binder object types with pointers should be offsets relative
 * to the start of the buffer. `ubinder_send_transaction` will take care of converting
 * them.
 * 
 */
typedef struct ubinder_object {
  ubinder_object_type type;
  size_t len;
  union {
    struct binder_object_header *object;
    void *blob;
  };
} ubinder_object;

typedef struct ubinder_transaction {
  uint32_t handle;
  binder_uintptr_t cookie;
  uint32_t code;
  uint32_t flags;
  struct ubinder_object **objs;
  size_t objcount;
} ubinder_transaction;

int ubinder_init(ubinder_state *state);
void ubinder_free(ubinder_state *state);
int ubinder_thread_init(ubinder_thread_state **out);
void ubinder_thread_free(ubinder_thread_state *tstate);
int ubinder_become_context_mgr(ubinder_state *state);
int ubinder_enter_looper(ubinder_state *state, ubinder_thread_state *tstate);
int ubinder_register_looper(ubinder_state *state, ubinder_thread_state *tstate);
int ubinder_write_read(ubinder_state *state, ubinder_thread_state *tstate, 
    void *buffer, size_t len);
int ubinder_send_transaction(ubinder_state *state, ubinder_thread_state *tstate, 
  ubinder_transaction *transaction);