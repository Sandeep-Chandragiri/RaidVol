#include "raid5.h"
#include "device.h"
#include "metadata.h"

#include <stdio.h>
#include <string.h>
/*
 * Return the parity disk for a stripe.
 *
 * Parity rotates between disks:
 *
 * Stripe 0 -> Disk 0
 * Stripe 1 -> Disk 1
 * Stripe 2 -> Disk 2
 * Stripe 3 -> Disk 0
 */
size_t raid5_parity_disk(
    size_t stripe
)
{
    return stripe % RAID5_DEVICE_COUNT;
}

/*
 * Map a virtual RAID-5 data block.
 *
 * Each stripe contains:
 *
 *     2 data blocks
 *     1 parity block
 *
 * Example:
 *
 * Stripe 0:
 *     Disk 0 -> Data
 *     Disk 1 -> Data
 *     Disk 2 -> Parity
 *
 * Stripe 1:
 *     Disk 0 -> Data
 *     Disk 1 -> Parity
 *     Disk 2 -> Data
 *
 * Stripe 2:
 *     Disk 0 -> Parity
 *     Disk 1 -> Data
 *     Disk 2 -> Data
 */
void raid5_map(
    size_t virtual_block,
    size_t *device,
    size_t *physical_block
)
{
    size_t stripe =
        virtual_block / (RAID5_DEVICE_COUNT - 1);

    size_t data_position =
        virtual_block % (RAID5_DEVICE_COUNT - 1);

    size_t parity_disk =
        raid5_parity_disk(stripe);

    size_t data_disk = 0;

    size_t data_seen = 0;

    for (size_t disk = 0;
         disk < RAID5_DEVICE_COUNT;
         disk++)
    {
        if (disk == parity_disk)
        {
            continue;
        }

        if (data_seen == data_position)
        {
            data_disk = disk;
            break;
        }

        data_seen++;
    }

    *device = data_disk;

    *physical_block = stripe;
}

/*
 * Calculate XOR parity.
 *
 * P = A XOR B
 */
void raid5_calculate_parity(
    const unsigned char *data_a,
    const unsigned char *data_b,
    unsigned char *parity,
    size_t size
)
{
    for (size_t i = 0; i < size; i++)
    {
        parity[i] =
            data_a[i] ^ data_b[i];
    }
}

/*
 * Rebuild missing data.
 *
 * Missing = Surviving XOR Parity
 */
void raid5_rebuild_data(
    const unsigned char *surviving_data,
    const unsigned char *parity,
    unsigned char *recovered_data,
    size_t size
)
{
    for (size_t i = 0; i < size; i++)
    {
        recovered_data[i] =
            surviving_data[i] ^ parity[i];
    }
}
/*
 * Write one virtual block to RAID-5.
 *
 * For a 3-disk RAID-5:
 *
 *     2 data blocks + 1 parity block
 *
 * Parity:
 *
 *     P = A XOR B
 */
int raid5_write(
    int fds[],
    size_t virtual_block,
    const void *buffer
)
{
    unsigned char other_data[RAID5_BLOCK_SIZE];
    unsigned char parity[RAID5_BLOCK_SIZE];

    size_t data_disk;
    size_t physical_block;

    size_t stripe =
        virtual_block / (RAID5_DEVICE_COUNT - 1);

    size_t data_position =
        virtual_block % (RAID5_DEVICE_COUNT - 1);

    size_t parity_disk =
        raid5_parity_disk(stripe);

    /*
     * Find where the requested virtual block lives.
     */
    raid5_map(
        virtual_block,
        &data_disk,
        &physical_block
    );

    /*
     * Find the other data block in this stripe.
     *
     * Example:
     *
     * Virtual 0 -> data position 0
     * Virtual 1 -> data position 1
     *
     * If writing Virtual 0,
     * we need Virtual 1 to calculate parity.
     */
    size_t other_virtual_block =
        stripe * (RAID5_DEVICE_COUNT - 1)
        + (1 - data_position);

    size_t other_disk;
    size_t other_physical_block;

    raid5_map(
        other_virtual_block,
        &other_disk,
        &other_physical_block
    );

    /*
     * Read the other data block.
     */
    ssize_t read_result =
        device_read(
            fds[other_disk],
            other_data,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET +
            (off_t)other_physical_block *
            RAID5_BLOCK_SIZE
        );

    if (read_result != RAID5_BLOCK_SIZE)
    {
        perror("raid5_write: reading other data block");
        return -1;
    }

    /*
     * Calculate new parity.
     *
     * P = New Data XOR Other Data
     */
    raid5_calculate_parity(
        (const unsigned char *)buffer,
        other_data,
        parity,
        RAID5_BLOCK_SIZE
    );

    /*
     * Write the new data block.
     */
    ssize_t data_written =
        device_write(
            fds[data_disk],
            buffer,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET +
            (off_t)physical_block *
            RAID5_BLOCK_SIZE
        );

    if (data_written != RAID5_BLOCK_SIZE)
    {
        perror("raid5_write: data");
        return -1;
    }

    /*
     * Write the new parity block.
     */
    ssize_t parity_written =
        device_write(
            fds[parity_disk],
            parity,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET +
            (off_t)physical_block *
            RAID5_BLOCK_SIZE
        );

    if (parity_written != RAID5_BLOCK_SIZE)
    {
        perror("raid5_write: parity");
        return -1;
    }

    printf(
        "WRITE: Virtual Block %zu -> "
        "Disk %zu, Physical Block %zu | "
        "Parity -> Disk %zu\n",
        virtual_block,
        data_disk,
        physical_block,
        parity_disk
    );

    return 0;
}


/*
 * Read one virtual block from RAID-5.
 */
int raid5_read(
    int fds[],
    size_t virtual_block,
    void *buffer
)
{
    size_t data_disk;
    size_t physical_block;

    /*
     * Determine physical location.
     */
    raid5_map(
        virtual_block,
        &data_disk,
        &physical_block
    );

    /*
     * Read data.
     */
    ssize_t result =
        device_read(
            fds[data_disk],
            buffer,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET +
            (off_t)physical_block *
            RAID5_BLOCK_SIZE
        );

    if (result != RAID5_BLOCK_SIZE)
    {
        perror("raid5_read");
        return -1;
    }

    printf(
        "READ : Virtual Block %zu -> "
        "Disk %zu, Physical Block %zu\n",
        virtual_block,
        data_disk,
        physical_block
    );

    return 0;
}
/*
 * RAID-5 degraded read.
 *
 * This function allows the volume to continue reading
 * when one disk has failed.
 *
 * For a 3-disk RAID-5:
 *
 *     Missing Data = Other Data XOR Parity
 *
 * Cases:
 *
 * 1. Requested data disk is healthy
 *      -> read normally.
 *
 * 2. Requested data disk has failed
 *      -> reconstruct from surviving data + parity.
 *
 * 3. Parity disk has failed
 *      -> requested data is still available,
 *         so read the data normally.
 */
int raid5_read_degraded(
    int fds[],
    size_t virtual_block,
    size_t failed_disk,
    void *buffer
)
{
    if (fds == NULL || buffer == NULL)
    {
        return -1;
    }

    if (failed_disk >= RAID5_DEVICE_COUNT)
    {
        printf(
            "raid5_read_degraded: invalid failed disk %zu\n",
            failed_disk
        );

        return -1;
    }

    size_t data_disk;
    size_t physical_block;

    /*
     * Determine where the requested virtual block
     * normally lives.
     */
    raid5_map(
        virtual_block,
        &data_disk,
        &physical_block
    );

    size_t stripe =
        virtual_block / (RAID5_DEVICE_COUNT - 1);

    size_t parity_disk =
        raid5_parity_disk(stripe);

    /*
     * Case 1:
     *
     * Requested data is on a healthy disk.
     *
     * We do not need reconstruction.
     */
    if (data_disk != failed_disk)
    {
        ssize_t result =
            device_read(
                fds[data_disk],
                buffer,
                RAID5_BLOCK_SIZE,
                RAIDVOL_DATA_OFFSET +
                (off_t)physical_block *
                RAID5_BLOCK_SIZE
            );

        if (result != RAID5_BLOCK_SIZE)
        {
            perror(
                "raid5_read_degraded: normal read"
            );

            return -1;
        }

        printf(
            "DEGRADED READ: Virtual Block %zu -> "
            "Disk %zu, Physical Block %zu "
            "(healthy data disk)\n",
            virtual_block,
            data_disk,
            physical_block
        );

        return 0;
    }

    /*
     * Case 2:
     *
     * Requested data disk has failed.
     *
     * We must reconstruct the missing block.
     */
    if (data_disk == failed_disk)
    {
        unsigned char surviving_data[
            RAID5_BLOCK_SIZE
        ];

        unsigned char parity[
            RAID5_BLOCK_SIZE
        ];

        /*
         * The failed data disk cannot be the parity disk
         * for the requested data block.
         */
        if (failed_disk == parity_disk)
        {
            printf(
                "raid5_read_degraded: internal mapping error\n"
            );

            return -1;
        }

        /*
         * Find the other data disk in this stripe.
         */
        size_t other_virtual_block;

        if (virtual_block % 2 == 0)
        {
            other_virtual_block =
                virtual_block + 1;
        }
        else
        {
            other_virtual_block =
                virtual_block - 1;
        }

        size_t other_disk;
        size_t other_physical_block;

        raid5_map(
            other_virtual_block,
            &other_disk,
            &other_physical_block
        );

        /*
         * Read surviving data.
         */
        ssize_t data_result =
            device_read(
                fds[other_disk],
                surviving_data,
                RAID5_BLOCK_SIZE,
                RAIDVOL_DATA_OFFSET +
                (off_t)other_physical_block *
                RAID5_BLOCK_SIZE
            );

        if (data_result != RAID5_BLOCK_SIZE)
        {
            perror(
                "raid5_read_degraded: surviving data"
            );

            return -1;
        }

        /*
         * Read parity.
         */
        ssize_t parity_result =
            device_read(
                fds[parity_disk],
                parity,
                RAID5_BLOCK_SIZE,
                RAIDVOL_DATA_OFFSET +
                (off_t)physical_block *
                RAID5_BLOCK_SIZE
            );

        if (parity_result != RAID5_BLOCK_SIZE)
        {
            perror(
                "raid5_read_degraded: parity"
            );

            return -1;
        }

        /*
         * Recover:
         *
         * Missing = Surviving XOR Parity
         */
        raid5_rebuild_data(
            surviving_data,
            parity,
            (unsigned char *)buffer,
            RAID5_BLOCK_SIZE
        );

        printf(
            "DEGRADED READ: Virtual Block %zu -> "
            "Disk %zu FAILED, reconstructed from "
            "Disk %zu + Parity Disk %zu\n",
            virtual_block,
            failed_disk,
            other_disk,
            parity_disk
        );

        return 0;
    }

    return -1;
}