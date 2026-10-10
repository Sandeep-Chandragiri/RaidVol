#define _POSIX_C_SOURCE 200809L

#include "raid1.h"
#include "device.h"
#include "metadata.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=================================\n");
    printf("       RaidVol RAID-1 Test\n");
    printf("=================================\n\n");

    const char *disk_paths[RAID1_DEVICE_COUNT] =
    {
        "disks/disk0.img",
        "disks/disk1.img"
    };

    int fds[RAID1_DEVICE_COUNT];

    /* Open Disk 0 */
    fds[0] = device_open(disk_paths[0]);

    if (fds[0] == -1)
    {
        printf("Failed to open disk 0\n");
        return 1;
    }

    printf("Opened Disk 0: %s\n", disk_paths[0]);

    /* Open Disk 1 */
    fds[1] = device_open(disk_paths[1]);

    if (fds[1] == -1)
    {
        printf("Failed to open disk 1\n");
        device_close(fds[0]);
        return 1;
    }

    printf("Opened Disk 1: %s\n\n", disk_paths[1]);

    char write_buffer[RAID1_BLOCK_SIZE];
    char read_buffer[RAID1_BLOCK_SIZE];

    memset(write_buffer, 0, sizeof(write_buffer));
    memset(read_buffer, 0, sizeof(read_buffer));

    strcpy(
        write_buffer,
        "RaidVol RAID-1 Test Data - Mirror Verification"
    );

    /*
     * =================================
     * TEST 1: MIRRORED WRITE
     * =================================
     */

    printf("Writing mirrored block...\n\n");

    if (raid1_write(fds, 0, write_buffer) != 0)
    {
        printf("RAID-1 write failed.\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    /*
     * =================================
     * TEST 2: NORMAL READ
     * =================================
     */

    printf("\nReading block normally...\n\n");

    if (raid1_read(fds, 0, read_buffer) != 0)
    {
        printf("RAID-1 normal read failed.\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    if (memcmp(
            write_buffer,
            read_buffer,
            RAID1_BLOCK_SIZE) == 0)
    {
        printf("RAID-1 NORMAL READ VERIFICATION: PASS\n");
    }
    else
    {
        printf("RAID-1 NORMAL READ VERIFICATION: FAIL\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    /*
     * =================================
     * TEST 3: VERIFY BOTH MIRROR COPIES
     * =================================
     */

    char disk0_buffer[RAID1_BLOCK_SIZE];
    char disk1_buffer[RAID1_BLOCK_SIZE];

    memset(disk0_buffer, 0, sizeof(disk0_buffer));
    memset(disk1_buffer, 0, sizeof(disk1_buffer));

    printf("\n---------------------------------\n");
    printf("Verifying physical mirror copies...\n");
    printf("---------------------------------\n\n");

    /*
     * RAID data starts after the metadata region.
     *
     * Virtual Block 0 -> RAIDVOL_DATA_OFFSET
     */
    ssize_t read0 = device_read(
        fds[0],
        disk0_buffer,
        RAID1_BLOCK_SIZE,
        RAIDVOL_DATA_OFFSET
    );

    ssize_t read1 = device_read(
        fds[1],
        disk1_buffer,
        RAID1_BLOCK_SIZE,
        RAIDVOL_DATA_OFFSET
    );

    if (read0 != RAID1_BLOCK_SIZE ||
        read1 != RAID1_BLOCK_SIZE)
    {
        printf("Failed to read physical mirror copies.\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    /*
     * Verify Disk 0.
     */
    if (memcmp(
            disk0_buffer,
            write_buffer,
            RAID1_BLOCK_SIZE) == 0)
    {
        printf("DISK 0 COPY: PASS\n");
    }
    else
    {
        printf("DISK 0 COPY: FAIL\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    /*
     * Verify Disk 1.
     */
    if (memcmp(
            disk1_buffer,
            write_buffer,
            RAID1_BLOCK_SIZE) == 0)
    {
        printf("DISK 1 COPY: PASS\n");
    }
    else
    {
        printf("DISK 1 COPY: FAIL\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    /*
     * Compare Disk 0 and Disk 1 directly.
     */
    if (memcmp(
            disk0_buffer,
            disk1_buffer,
            RAID1_BLOCK_SIZE) == 0)
    {
        printf("RAID-1 MIRROR CONSISTENCY: PASS\n");
    }
    else
    {
        printf("RAID-1 MIRROR CONSISTENCY: FAIL\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    /*
     * =================================
     * TEST 4: SIMULATE DISK 0 FAILURE
     * =================================
     */

    printf("\n---------------------------------\n");
    printf("Simulating Disk 0 failure...\n");
    printf("---------------------------------\n\n");

    device_close(fds[0]);

    /*
     * Mark Disk 0 as unavailable.
     */
    fds[0] = -1;

    memset(
        read_buffer,
        0,
        sizeof(read_buffer)
    );

    printf("Reading block after Disk 0 failure...\n\n");

    /*
     * raid1_read() should automatically
     * fall back to Disk 1.
     */
    if (raid1_read(
            fds,
            0,
            read_buffer) != 0)
    {
        printf("RAID-1 degraded read failed.\n");

        device_close(fds[1]);

        return 1;
    }

    /*
     * Verify recovered data.
     */
    if (memcmp(
            write_buffer,
            read_buffer,
            RAID1_BLOCK_SIZE) == 0)
    {
        printf("\nRAID-1 FAILURE RECOVERY: PASS\n");
        printf("Data successfully recovered from Disk 1 mirror.\n");
    }
    else
    {
        printf("\nRAID-1 FAILURE RECOVERY: FAIL\n");

        device_close(fds[1]);

        return 1;
    }

    /*
     * Close remaining healthy disk.
     */
    device_close(fds[1]);

    /*
     * =================================
     * FINAL RESULT
     * =================================
     */

    printf("\n=================================\n");
    printf("       RAID-1 TEST COMPLETE\n");
    printf("=================================\n");

    return 0;
}
