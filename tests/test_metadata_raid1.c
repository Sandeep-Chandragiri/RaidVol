#define _POSIX_C_SOURCE 200809L

#include "metadata.h"
#include "device.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=================================\n");
    printf("   RaidVol RAID-1 Metadata Test\n");
    printf("=================================\n\n");

    const char *disk_paths[2] =
    {
        "disks/disk0.img",
        "disks/disk1.img"
    };

    int fds[2];

    /*
     * Open both RAID-1 disks.
     */
    fds[0] = device_open(disk_paths[0]);

    if (fds[0] == -1)
    {
        printf("Failed to open Disk 0\n");
        return 1;
    }

    printf("Opened Disk 0: %s\n", disk_paths[0]);

    fds[1] = device_open(disk_paths[1]);

    if (fds[1] == -1)
    {
        printf("Failed to open Disk 1\n");
        device_close(fds[0]);
        return 1;
    }

    printf("Opened Disk 1: %s\n\n", disk_paths[1]);

    /*
     * Create RAID-1 metadata.
     */
    RaidVolumeMetadata metadata;

    memset(&metadata, 0, sizeof(metadata));

    memcpy(
        metadata.magic,
        RAIDVOL_MAGIC,
        strlen(RAIDVOL_MAGIC)
    );

    metadata.version = RAIDVOL_VERSION;
    metadata.raid_level = 1;
    metadata.device_count = 2;
    metadata.block_size = 4096;
    metadata.volume_size = 1024 * 1024;

    /*
     * Write metadata to BOTH disks.
     */
    printf("Writing metadata to Disk 0...\n");

    if (metadata_write(fds[0], &metadata) != 0)
    {
        printf("Disk 0 metadata write: FAIL\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    printf("Disk 0 metadata write: PASS\n");

    printf("Writing metadata to Disk 1...\n");

    if (metadata_write(fds[1], &metadata) != 0)
    {
        printf("Disk 1 metadata write: FAIL\n");

        device_close(fds[0]);
        device_close(fds[1]);

        return 1;
    }

    printf("Disk 1 metadata write: PASS\n\n");

    /*
     * Close both disks.
     */
    printf("Closing both disks...\n");

    device_close(fds[0]);
    device_close(fds[1]);

    printf("Disks detached successfully.\n\n");

    /*
     * Reopen Disk 1 only.
     *
     * This simulates Disk 0 being unavailable.
     */
    printf("Reopening Disk 1...\n");

    int fd = device_open(disk_paths[1]);

    if (fd == -1)
    {
        printf("Failed to reopen Disk 1\n");
        return 1;
    }

    printf("Disk 1 reattached successfully.\n\n");

    /*
     * Read metadata after reattachment.
     */
    RaidVolumeMetadata loaded_metadata;

    memset(
        &loaded_metadata,
        0,
        sizeof(loaded_metadata)
    );

    printf("Reading metadata from reattached Disk 1...\n");

    if (metadata_read(fd, &loaded_metadata) != 0)
    {
        printf("Metadata read: FAIL\n");

        device_close(fd);
        return 1;
    }

    printf("Metadata read: PASS\n\n");

    /*
     * Display metadata.
     */
    printf("Recovered Metadata:\n");
    printf("-------------------\n");

    printf(
        "Magic        : %s\n",
        loaded_metadata.magic
    );

    printf(
        "Version      : %u\n",
        loaded_metadata.version
    );

    printf(
        "RAID Level   : %u\n",
        loaded_metadata.raid_level
    );

    printf(
        "Device Count : %u\n",
        loaded_metadata.device_count
    );

    printf(
        "Block Size   : %u\n",
        loaded_metadata.block_size
    );

    printf(
        "Volume Size  : %llu bytes\n",
        (unsigned long long)loaded_metadata.volume_size
    );

    printf("\n");

    /*
     * Validate recovered metadata.
     */
    printf("Validating recovered metadata...\n");

    if (metadata_validate(&loaded_metadata) != 0)
    {
        printf("Metadata validation: FAIL\n");

        device_close(fd);
        return 1;
    }

    printf("Metadata validation: PASS\n");

    /*
     * Compare recovered metadata with original.
     */
    if (memcmp(
            &metadata,
            &loaded_metadata,
            sizeof(RaidVolumeMetadata)) != 0)
    {
        printf("Metadata persistence: FAIL\n");

        device_close(fd);
        return 1;
    }

    printf("Metadata persistence: PASS\n");

    device_close(fd);

    printf("\n=================================\n");
    printf(" RAID-1 METADATA TEST: PASS\n");
    printf("=================================\n");

    return 0;
}
