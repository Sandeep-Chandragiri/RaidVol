#define _POSIX_C_SOURCE 200809L

#include "metadata.h"
#include "device.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void)
{
    printf("=================================\n");
    printf("      RaidVol Metadata Test\n");
    printf("=================================\n\n");

    const char *disk_path = "disks/disk0.img";

    /*
     * Open disk image.
     */
    int fd = device_open(disk_path);

    if (fd == -1)
    {
        printf("Failed to open disk.\n");
        return 1;
    }

    printf("Opened: %s\n\n", disk_path);

    /*
     * Create metadata.
     */
    RaidVolumeMetadata metadata;

    memset(&metadata, 0, sizeof(metadata));

    memcpy(
        metadata.magic,
        RAIDVOL_MAGIC,
        strlen(RAIDVOL_MAGIC)
    );

    metadata.version = RAIDVOL_VERSION;

    /*
     * RAID-1 configuration for this test.
     */
    metadata.raid_level = 1;

    metadata.device_count = 2;

    metadata.block_size = 4096;

    /*
     * Our disk images are 1 MiB.
     */
    metadata.volume_size = 1024 * 1024;

    /*
     * Write metadata.
     */
    printf("Writing metadata...\n");

    if (metadata_write(fd, &metadata) != 0)
    {
        printf("Metadata write failed.\n");
        device_close(fd);
        return 1;
    }

    printf("Metadata write: PASS\n\n");

    /*
     * Read metadata back.
     */
    RaidVolumeMetadata loaded_metadata;

    memset(
        &loaded_metadata,
        0,
        sizeof(loaded_metadata)
    );

    printf("Reading metadata...\n");

    if (metadata_read(fd, &loaded_metadata) != 0)
    {
        printf("Metadata read failed.\n");
        device_close(fd);
        return 1;
    }

    printf("Metadata read: PASS\n\n");

    /*
     * Display metadata.
     */
    printf("Metadata contents:\n");
    printf("-----------------\n");

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
     * Validate metadata.
     */
    printf("Validating metadata...\n");

    if (metadata_validate(&loaded_metadata) != 0)
    {
        printf("Metadata validation: FAIL\n");
        device_close(fd);
        return 1;
    }

    printf("Metadata validation: PASS\n");

    /*
     * Compare original and loaded metadata.
     */
    if (memcmp(
            &metadata,
            &loaded_metadata,
            sizeof(RaidVolumeMetadata)) != 0)
    {
        printf("Metadata round-trip: FAIL\n");
        device_close(fd);
        return 1;
    }

    printf("Metadata round-trip: PASS\n");

    device_close(fd);

    printf("\n=================================\n");
    printf("    METADATA TEST COMPLETE\n");
    printf("=================================\n");

    return 0;
}
