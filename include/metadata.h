#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>

#define RAIDVOL_MAGIC "RAIDVOL"
#define RAIDVOL_VERSION 1

#define RAIDVOL_METADATA_SIZE 4096
#define RAIDVOL_DATA_OFFSET 4096

typedef struct
{
    char magic[8];

    uint32_t version;

    uint32_t raid_level;

    uint32_t device_count;

    uint32_t block_size;

    uint64_t volume_size;

} RaidVolumeMetadata;

int metadata_write(
    int fd,
    const RaidVolumeMetadata *metadata
);

int metadata_read(
    int fd,
    RaidVolumeMetadata *metadata
);

int metadata_validate(
    const RaidVolumeMetadata *metadata
);

#endif
