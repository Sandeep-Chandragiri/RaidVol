#ifndef RAID5_H
#define RAID5_H

#include <stddef.h>

/* RAID-5 configuration */

#define RAID5_DEVICE_COUNT 3
#define RAID5_BLOCK_SIZE 4096


/* RAID-5 mapping */

void raid5_map(
    size_t virtual_block,
    size_t *device,
    size_t *physical_block
);


/* Return parity disk for a stripe */

size_t raid5_parity_disk(
    size_t stripe
);


/* XOR parity calculation */

void raid5_calculate_parity(
    const unsigned char *data_a,
    const unsigned char *data_b,
    unsigned char *parity,
    size_t size
);


/* Rebuild missing data */

void raid5_rebuild_data(
    const unsigned char *surviving_data,
    const unsigned char *parity,
    unsigned char *recovered_data,
    size_t size
);


/* Real RAID-5 disk I/O */

int raid5_write(
    int fds[],
    size_t virtual_block,
    const void *buffer
);

int raid5_read(
    int fds[],
    size_t virtual_block,
    void *buffer
);


/*
 * RAID-5 degraded read.
 *
 * failed_disk:
 *     Index of the failed disk.
 *
 * If the requested block is on the failed disk,
 * it is reconstructed using:
 *
 *     Missing = Surviving Data XOR Parity
 */
int raid5_read_degraded(
    int fds[],
    size_t virtual_block,
    size_t failed_disk,
    void *buffer
);

#endif