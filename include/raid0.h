#ifndef RAID0_H
#define RAID0_H

#include <stddef.h>
#include <sys/types.h>

#define RAID0_BLOCK_SIZE 4096
#define RAID0_DEVICE_COUNT 3

void raid0_map(
    size_t virtual_block,
    size_t *device,
    size_t *physical_block
);

int raid0_write(
    int fds[],
    size_t virtual_block,
    const void *buffer
);

int raid0_read(
    int fds[],
    size_t virtual_block,
    void *buffer
);

#endif
