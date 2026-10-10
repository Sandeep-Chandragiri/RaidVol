#define _POSIX_C_SOURCE 200809L

#include "raid0.h"
#include "metadata.h"
#include "device.h"

#include <stdio.h>

void raid0_map(
    size_t virtual_block,
    size_t *device,
    size_t *physical_block)
{
    *device = virtual_block % RAID0_DEVICE_COUNT;
    *physical_block = virtual_block / RAID0_DEVICE_COUNT;
}

int raid0_write(
    int fds[],
    size_t virtual_block,
    const void *buffer)
{
    size_t device;
    size_t physical_block;

    raid0_map(
        virtual_block,
        &device,
        &physical_block
    );


off_t offset =
    RAIDVOL_DATA_OFFSET +
    (off_t)physical_block * RAID0_BLOCK_SIZE;

    ssize_t result = device_write(
        fds[device],
        buffer,
        RAID0_BLOCK_SIZE,
        offset
    );

    if (result != RAID0_BLOCK_SIZE)
    {
        perror("raid0_write");
        return -1;
    }

    printf(
        "WRITE: Virtual Block %zu -> "
        "Disk %zu -> Offset %lld\n",
        virtual_block,
        device,
        (long long)offset
    );

    return 0;
}

int raid0_read(
    int fds[],
    size_t virtual_block,
    void *buffer)
{
    size_t device;
    size_t physical_block;

    raid0_map(
        virtual_block,
        &device,
        &physical_block
    );

off_t offset =
    RAIDVOL_DATA_OFFSET +
    (off_t)physical_block * RAID0_BLOCK_SIZE;

    ssize_t result = device_read(
        fds[device],
        buffer,
        RAID0_BLOCK_SIZE,
        offset
    );

    if (result != RAID0_BLOCK_SIZE)
    {
        perror("raid0_read");
        return -1;
    }

    printf(
        "READ : Virtual Block %zu -> "
        "Disk %zu -> Offset %lld\n",
        virtual_block,
        device,
        (long long)offset
    );

    return 0;
}
