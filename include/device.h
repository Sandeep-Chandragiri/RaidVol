#ifndef DEVICE_H
#define DEVICE_H

#include <stddef.h>
#include <sys/types.h>

/*
 * Open a backing disk.
 *
 * Returns:
 *   file descriptor >= 0 on success
 *   -1 on failure
 */
int device_open(const char *path);

/*
 * Read exactly size bytes from a device.
 */
ssize_t device_read(
    int fd,
    void *buffer,
    size_t size,
    off_t offset
);

/*
 * Write exactly size bytes to a device.
 */
ssize_t device_write(
    int fd,
    const void *buffer,
    size_t size,
    off_t offset
);

/*
 * Close a device.
 */
void device_close(int fd);

#endif
