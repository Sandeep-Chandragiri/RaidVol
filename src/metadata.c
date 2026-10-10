#define _POSIX_C_SOURCE 200809L

#include "metadata.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define METADATA_OFFSET 0

int metadata_write(
    int fd,
    const RaidVolumeMetadata *metadata
)
{
    if (fd < 0 || metadata == NULL)
    {
        return -1;
    }

    ssize_t written = pwrite(
        fd,
        metadata,
        sizeof(RaidVolumeMetadata),
        METADATA_OFFSET
    );

    if (written != sizeof(RaidVolumeMetadata))
    {
        perror("metadata_write");
        return -1;
    }

    return 0;
}

int metadata_read(
    int fd,
    RaidVolumeMetadata *metadata
)
{
    if (fd < 0 || metadata == NULL)
    {
        return -1;
    }

    ssize_t bytes_read = pread(
        fd,
        metadata,
        sizeof(RaidVolumeMetadata),
        METADATA_OFFSET
    );

    if (bytes_read != sizeof(RaidVolumeMetadata))
    {
        perror("metadata_read");
        return -1;
    }

    return 0;
}

int metadata_validate(
    const RaidVolumeMetadata *metadata
)
{
    if (metadata == NULL)
    {
        return -1;
    }

    if (memcmp(
            metadata->magic,
            RAIDVOL_MAGIC,
            strlen(RAIDVOL_MAGIC)) != 0)
    {
        return -1;
    }

    if (metadata->version != RAIDVOL_VERSION)
    {
        return -1;
    }

    if (metadata->device_count == 0)
    {
        return -1;
    }

    if (metadata->block_size == 0)
    {
        return -1;
    }

    if (metadata->volume_size == 0)
    {
        return -1;
    }

    return 0;
}
