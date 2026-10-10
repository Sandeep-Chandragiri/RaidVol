#define _POSIX_C_SOURCE 200809L

#include "raid1.h"
#include "device.h"
#include "metadata.h"

#include <stdio.h>

/* * RAID-1 write. * * The same block is written to * both mirror devices. */
int raid1_write(
    int fds[],
    size_t virtual_block,
    const void *buffer)
{

off_t offset =
    RAIDVOL_DATA_OFFSET +
    (off_t)virtual_block * RAID1_BLOCK_SIZE;

    /*
     * Write to mirror 0.
     */
    ssize_t first_write = device_write(
        fds[0],
        buffer,
        RAID1_BLOCK_SIZE,
        offset
    );

    if (first_write != RAID1_BLOCK_SIZE)
    {
        perror("raid1_write: disk0");
        return -1;
    }

    /*
     * Write to mirror 1.
     */
    ssize_t second_write = device_write(
        fds[1],
        buffer,
        RAID1_BLOCK_SIZE,
        offset
    );

    if (second_write != RAID1_BLOCK_SIZE)
    {
        perror("raid1_write: disk1");
        return -1;
    }

    printf(
        "WRITE: Virtual Block %zu -> "
        "Disk 0 + Disk 1 -> Offset %lld\n",
        virtual_block,
        (long long)offset
    );

    return 0;
}

/*
 * RAID-1 read.
 *
 * For now we read from the first mirror.
 */
int raid1_read(int fds[], size_t virtual_block, void *buffer)
{

off_t offset =
    RAIDVOL_DATA_OFFSET +
    (off_t)virtual_block * RAID1_BLOCK_SIZE;

    /* Try Disk 0 first */
    ssize_t result = device_read(
        fds[0],
        buffer,
        RAID1_BLOCK_SIZE,
        offset
    );

    if (result == RAID1_BLOCK_SIZE)
    {
        printf(
            "READ : Virtual Block %zu -> Disk 0 -> Offset %lld\n",
            virtual_block,
            (long long)offset
        );

        return 0;
    }

    printf("Disk 0 unavailable. Trying Disk 1...\n");

    /* Disk 0 failed, use mirror Disk 1 */
    result = device_read(
        fds[1],
        buffer,
        RAID1_BLOCK_SIZE,
        offset
    );

    if (result == RAID1_BLOCK_SIZE)
    {
        printf(
            "READ : Virtual Block %zu -> Disk 1 (MIRROR) -> Offset %lld\n",
            virtual_block,
            (long long)offset
        );

        return 0;
    }

    perror("raid1_read: both disks failed");
    return -1;
}
