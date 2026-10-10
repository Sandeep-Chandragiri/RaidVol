#define _POSIX_C_SOURCE 200809L

#include "device.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int device_open(const char *path)
{
    int fd = open(path, O_RDWR);

    if (fd == -1)
    {
        perror("device_open");
    }

    return fd;
}

ssize_t device_read(
    int fd,
    void *buffer,
    size_t size,
    off_t offset)
{
    return pread(fd, buffer, size, offset);
}

ssize_t device_write(
    int fd,
    const void *buffer,
    size_t size,
    off_t offset)
{
    return pwrite(fd, buffer, size, offset);
}

void device_close(int fd)
{
    close(fd);
}
