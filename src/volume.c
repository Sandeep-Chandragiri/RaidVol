#include "volume.h"
#include "device.h"
#include "metadata.h"

#include <stdio.h>
#include <string.h>
#include "raid5.h"

int volume_open(
    RaidVolume *volume,
    const char *paths[],
    size_t device_count,
    size_t block_size,
    RaidLevel raid_level
)
{
    if (volume == NULL || paths == NULL)
    {
        return -1;
    }

    if (device_count == 0 ||
        device_count > VOLUME_MAX_DEVICES)
    {
        return -1;
    }

    /*
     * Initialize all file descriptors.
     */
    for (size_t i = 0; i < VOLUME_MAX_DEVICES; i++)
    {
        volume->fds[i] = -1;
    }

    volume->device_count = device_count;
    volume->block_size = block_size;
    volume->raid_level = raid_level;

    for (size_t i = 0; i < device_count; i++)
    {
        volume->fds[i] = device_open(paths[i]);

        if (volume->fds[i] == -1)
        {
            printf("Failed to open Disk %zu\n", i);

            for (size_t j = 0; j < i; j++)
            {
                device_close(volume->fds[j]);
                volume->fds[j] = -1;
            }

            return -1;
        }

        printf(
            "Opened Disk %zu: %s\n",
            i,
            paths[i]
        );
    }

    return 0;
}

int volume_open_from_metadata(
    RaidVolume *volume,
    const char *paths[],
    size_t device_count
)
{
    if (volume == NULL || paths == NULL)
    {
        return -1;
    }

    if (device_count == 0 ||
        device_count > VOLUME_MAX_DEVICES)
    {
        return -1;
    }

    /*
     * Initialize file descriptors.
     */
    for (size_t i = 0; i < VOLUME_MAX_DEVICES; i++)
    {
        volume->fds[i] = -1;
    }

    /*
     * We need to open at least one disk first
     * so that we can discover the metadata.
     */
    int first_fd = device_open(paths[0]);

    if (first_fd == -1)
    {
        printf("Failed to open Disk 0\n");
        return -1;
    }

    volume->fds[0] = first_fd;

    RaidVolumeMetadata metadata;

    memset(
        &metadata,
        0,
        sizeof(metadata)
    );

    /*
     * Read metadata from the first disk.
     */
    if (metadata_read(first_fd, &metadata) != 0)
    {
        printf("Failed to read volume metadata.\n");

        device_close(first_fd);
        volume->fds[0] = -1;

        return -1;
    }

    /*
     * Validate metadata.
     */
    if (metadata_validate(&metadata) != 0)
    {
        printf("Invalid volume metadata.\n");

        device_close(first_fd);
        volume->fds[0] = -1;

        return -1;
    }

    /*
     * Make sure the metadata agrees with the
     * number of paths supplied by the caller.
     */
    if (metadata.device_count != device_count)
    {
        printf(
            "Device count mismatch: metadata=%u, supplied=%zu\n",
            metadata.device_count,
            device_count
        );

        device_close(first_fd);
        volume->fds[0] = -1;

        return -1;
    }

    /*
     * Validate supported RAID levels.
     */
if (metadata.raid_level != RAID_LEVEL_0 &&
    metadata.raid_level != RAID_LEVEL_1 &&
    metadata.raid_level != RAID_LEVEL_5)
{
    printf("Unsupported RAID level.\n");

    for (size_t i = 0; i < device_count; i++)
    {
        if (volume->fds[i] >= 0)
        {
            device_close(volume->fds[i]);
            volume->fds[i] = -1;
        }
    }

    return -1;
}
    /*
     * Load discovered configuration.
     */
    volume->metadata = metadata;

    volume->device_count =
        metadata.device_count;

    volume->block_size =
        metadata.block_size;

    volume->raid_level =
        (RaidLevel)metadata.raid_level;

    printf("Metadata discovered successfully.\n");

    printf(
        "Detected RAID Level: %u\n",
        metadata.raid_level
    );

    printf(
        "Detected Device Count: %u\n",
        metadata.device_count
    );

    printf(
        "Detected Block Size: %u\n",
        metadata.block_size
    );

    /*
     * Open the remaining disks.
     */
    for (size_t i = 1; i < device_count; i++)
    {
        volume->fds[i] = device_open(paths[i]);

        if (volume->fds[i] == -1)
        {
            printf("Failed to open Disk %zu\n", i);

            for (size_t j = 0; j < i; j++)
            {
                if (volume->fds[j] >= 0)
                {
                    device_close(volume->fds[j]);
                    volume->fds[j] = -1;
                }
            }

            return -1;
        }

        printf(
            "Opened Disk %zu: %s\n",
            i,
            paths[i]
        );
    }

    /*
     * Verify metadata on every remaining disk.
     *
     * This ensures all members belong to the
     * same RaidVol volume configuration.
     */
    for (size_t i = 1; i < device_count; i++)
    {
        RaidVolumeMetadata member_metadata;

        memset(
            &member_metadata,
            0,
            sizeof(member_metadata)
        );

        if (metadata_read(
                volume->fds[i],
                &member_metadata) != 0)
        {
            printf(
                "Failed to read metadata from Disk %zu\n",
                i
            );

            volume_close(volume);
            return -1;
        }

        if (metadata_validate(&member_metadata) != 0)
        {
            printf(
                "Invalid metadata on Disk %zu\n",
                i
            );

            volume_close(volume);
            return -1;
        }

        if (member_metadata.raid_level != metadata.raid_level ||
            member_metadata.device_count != metadata.device_count ||
            member_metadata.block_size != metadata.block_size ||
            member_metadata.volume_size != metadata.volume_size)
        {
            printf(
                "Metadata mismatch on Disk %zu\n",
                i
            );

            volume_close(volume);
            return -1;
        }
    }

    printf("All disk metadata matches.\n");

    return 0;
}

int volume_write_metadata(RaidVolume *volume)
{
    if (volume == NULL)
    {
        return -1;
    }

    RaidVolumeMetadata metadata;

    memset(
        &metadata,
        0,
        sizeof(metadata)
    );

    memcpy(
        metadata.magic,
        RAIDVOL_MAGIC,
        strlen(RAIDVOL_MAGIC)
    );

    metadata.version = RAIDVOL_VERSION;

    metadata.raid_level =
        (uint32_t)volume->raid_level;

    metadata.device_count =
        (uint32_t)volume->device_count;

    metadata.block_size =
        (uint32_t)volume->block_size;

    metadata.volume_size =
        1024 * 1024;

    volume->metadata = metadata;

    for (size_t i = 0;
         i < volume->device_count;
         i++)
    {
        if (metadata_write(
                volume->fds[i],
                &metadata) != 0)
        {
            printf(
                "Metadata write failed on Disk %zu\n",
                i
            );

            return -1;
        }
    }

    return 0;
}

int volume_read_metadata(RaidVolume *volume)
{
    if (volume == NULL)
    {
        return -1;
    }

    RaidVolumeMetadata metadata;

    memset(
        &metadata,
        0,
        sizeof(metadata)
    );

    for (size_t i = 0;
         i < volume->device_count;
         i++)
    {
        if (volume->fds[i] < 0)
        {
            continue;
        }

        if (metadata_read(
                volume->fds[i],
                &metadata) == 0)
        {
            if (metadata_validate(&metadata) == 0)
            {
                volume->metadata = metadata;

                volume->raid_level =
                    (RaidLevel)metadata.raid_level;

                volume->device_count =
                    metadata.device_count;

                volume->block_size =
                    metadata.block_size;

                printf(
                    "Metadata loaded from Disk %zu\n",
                    i
                );

                return 0;
            }
        }
    }

    printf("No valid RaidVol metadata found.\n");

    return -1;
}

void volume_close(RaidVolume *volume)
{
    if (volume == NULL)
    {
        return;
    }

    for (size_t i = 0;
         i < VOLUME_MAX_DEVICES;
         i++)
    {
        if (volume->fds[i] >= 0)
        {
            device_close(volume->fds[i]);
            volume->fds[i] = -1;
        }
    }
}

int volume_read_degraded(
    RaidVolume *volume,
    size_t virtual_block,
    size_t failed_disk,
    void *buffer
)
{
    if (volume == NULL || buffer == NULL)
    {
        return -1;
    }

    if (volume->raid_level != RAID_LEVEL_5)
    {
        printf(
            "Degraded read is supported only for RAID-5.\n"
        );

        return -1;
    }

    if (volume->device_count != RAID5_DEVICE_COUNT)
    {
        printf(
            "RAID-5 requires exactly %d devices.\n",
            RAID5_DEVICE_COUNT
        );

        return -1;
    }

    if (failed_disk >= volume->device_count)
    {
        printf(
            "Invalid failed disk: %zu\n",
            failed_disk
        );

        return -1;
    }

    /*
     * Pass the RAID-5 request to the RAID-5 layer.
     */
    return raid5_read_degraded(
        volume->fds,
        virtual_block,
        failed_disk,
        buffer
    );
}
