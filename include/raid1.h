#ifndef RAID1_H
#define RAID1_H

#include <stddef.h>

#define RAID1_BLOCK_SIZE 4096
#define RAID1_DEVICE_COUNT 2

/*
 * Write one virtual block to both mirror devices.
 */
int raid1_write(
    int fds[],
    size_t virtual_block,
    const void *buffer
);

/*
 * Read one virtual block from a mirror device.
 */
int raid1_read(
    int fds[],
    size_t virtual_block,
    void *buffer
);

#endif
