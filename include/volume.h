#ifndef VOLUME_H
#define VOLUME_H

#include <stddef.h>

#include "metadata.h"

#define VOLUME_MAX_DEVICES 3

typedef enum
{
    RAID_LEVEL_0 = 0,
    RAID_LEVEL_1 = 1,
    RAID_LEVEL_5 = 5

} RaidLevel;

typedef struct
{
    int fds[VOLUME_MAX_DEVICES];

    size_t device_count;
    size_t block_size;

    RaidLevel raid_level;

    RaidVolumeMetadata metadata;

} RaidVolume;

/*
 * Open a volume using an explicitly supplied configuration.
 */
int volume_open(
    RaidVolume *volume,
    const char *paths[],
    size_t device_count,
    size_t block_size,
    RaidLevel raid_level
);

/*
 * Open a volume by discovering its configuration
 * from the metadata stored on the disk images.
 */
int volume_open_from_metadata(
    RaidVolume *volume,
    const char *paths[],
    size_t device_count
);

int volume_write_metadata(
    RaidVolume *volume
);

int volume_read_metadata(
    RaidVolume *volume
);

/*
 * Read from a RAID-5 volume while one disk is failed.
 */
int volume_read_degraded(
    RaidVolume *volume,
    size_t virtual_block,
    size_t failed_disk,
    void *buffer
);

void volume_close(RaidVolume *volume);

#endif
