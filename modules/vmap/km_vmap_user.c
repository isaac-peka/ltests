#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stddef.h>

void main()
{
    int fd = open("/dev/km_vmap", O_RDWR);

    if (fd < 0) {
        perror("Failed to open the file");
        return;
    }

    if (ioctl(fd, 0xff, NULL) < 0) {
        perror("Failed to ioctl");
        return;
    }  
}
