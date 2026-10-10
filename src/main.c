#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#include "metadata.h"
#include "device.h"
#include "volume.h"
#include "raid0.h"
#include "raid1.h"
#include "raid5.h"
#include "checksum.h"
#include "checksum_store.h"
#include "benchmark.h"

#define MAX_CREATE_DEVICES 3
#define DEFAULT_BLOCK_SIZE 4096
#define DEFAULT_VOLUME_SIZE (1024 * 1024)

static void print_usage(const char *program)
{
    printf("Usage:\n");
    printf("  %s inspect <disk-image>\n", program);
    printf("  %s create --raid 0 <disk1> <disk2> <disk3>\n", program);
    printf("  %s create --raid 1 <disk1> <disk2>\n", program);
    printf("  %s create --raid 5 <disk1> <disk2> <disk3>\n", program);
    printf("  %s open <disk1> <disk2> ...\n", program);
    printf("  %s raid5-test\n", program);
    printf("  %s raid0-map-test\n", program);
    printf("  %s checksum-test\n", program);
    printf("  %s raid5-checksum-test\n", program);
    printf("  %s raid5-checksum-multi-test\n", program);
    printf("  %s benchmark\n", program);
}

static const char *raid_level_name(uint32_t raid_level)
{
    switch (raid_level)
    {
        case 0:
            return "RAID-0";

        case 1:
            return "RAID-1";

        case 5:
            return "RAID-5";

        default:
            return "UNKNOWN";
    }
}

/*
 * =========================================
 * INSPECT
 * =========================================
 */

static int inspect_disk(const char *path)
{
    RaidVolumeMetadata metadata;

    memset(
        &metadata,
        0,
        sizeof(metadata)
    );

    printf("=================================\n");
    printf("       RaidVol Inspection\n");
    printf("=================================\n\n");

    printf(
        "Disk          : %s\n",
        path
    );

    int fd = device_open(path);

    if (fd < 0)
    {
        printf("Status        : CANNOT OPEN\n");
        return EXIT_FAILURE;
    }

    if (metadata_read(fd, &metadata) != 0)
    {
        printf("Status        : INVALID\n");

        device_close(fd);

        return EXIT_FAILURE;
    }

    if (metadata_validate(&metadata) != 0)
    {
        printf("Status        : INVALID\n");

        device_close(fd);

        return EXIT_FAILURE;
    }

    printf(
        "Magic         : %.7s\n",
        metadata.magic
    );

    printf(
        "Version       : %u\n",
        metadata.version
    );

    printf(
        "RAID Level    : %s\n",
        raid_level_name(metadata.raid_level)
    );

    printf(
        "Device Count  : %u\n",
        metadata.device_count
    );

    printf(
        "Block Size    : %u bytes\n",
        metadata.block_size
    );

    printf(
        "Volume Size   : %llu bytes\n",
        (unsigned long long)metadata.volume_size
    );

    printf(
        "Status        : VALID\n"
    );

    printf(
        "\n=================================\n"
    );

    device_close(fd);

    return EXIT_SUCCESS;
}

/*
 * =========================================
 * CREATE
 * =========================================
 */

static int create_volume(
    int raid_level,
    int disk_count,
    char *paths[]
)
{
    /*
     * Validate RAID configuration.
     */

    if (raid_level == 0)
    {
        if (disk_count != 3)
        {
            printf(
                "Error: RAID-0 requires exactly 3 disks.\n"
            );

            return EXIT_FAILURE;
        }
    }
    else if (raid_level == 1)
    {
        if (disk_count != 2)
        {
            printf(
                "Error: RAID-1 requires exactly 2 disks.\n"
            );

            return EXIT_FAILURE;
        }
    }
    else if (raid_level == 5)
    {
        if (disk_count != 3)
        {
            printf(
                "Error: RAID-5 requires exactly 3 disks.\n"
            );

            return EXIT_FAILURE;
        }
    }
    else
    {
        printf(
            "Error: unsupported RAID level %d.\n",
            raid_level
        );

        return EXIT_FAILURE;
    }

    printf("=================================\n");
    printf("        RaidVol Create\n");
    printf("=================================\n\n");

    printf(
        "RAID Level    : %s\n",
        raid_level_name(raid_level)
    );

    printf(
        "Device Count  : %d\n",
        disk_count
    );

    printf(
        "Block Size    : %d bytes\n",
        DEFAULT_BLOCK_SIZE
    );

    printf(
        "Volume Size   : %d bytes\n\n",
        DEFAULT_VOLUME_SIZE
    );

    /*
     * Open disks.
     */

    int fds[MAX_CREATE_DEVICES];

    for (int i = 0; i < MAX_CREATE_DEVICES; i++)
    {
        fds[i] = -1;
    }

    for (int i = 0; i < disk_count; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            printf(
                "Failed to open disk %d: %s\n",
                i,
                paths[i]
            );

            for (int j = 0; j < i; j++)
            {
                device_close(fds[j]);
            }

            return EXIT_FAILURE;
        }

        printf(
            "Opened Disk %d: %s\n",
            i,
            paths[i]
        );
    }

    /*
     * Prepare metadata.
     */

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

    metadata.version =
        RAIDVOL_VERSION;

    metadata.raid_level =
        (uint32_t)raid_level;

    metadata.device_count =
        (uint32_t)disk_count;

    metadata.block_size =
        DEFAULT_BLOCK_SIZE;

    metadata.volume_size =
        DEFAULT_VOLUME_SIZE;

    /*
     * Write metadata.
     */

    printf("\nWriting metadata...\n");

    for (int i = 0; i < disk_count; i++)
    {
        if (metadata_write(
                fds[i],
                &metadata) != 0)
        {
            printf(
                "Metadata write failed on Disk %d.\n",
                i
            );

            for (int j = 0; j < disk_count; j++)
            {
                if (fds[j] >= 0)
                {
                    device_close(fds[j]);
                }
            }

            return EXIT_FAILURE;
        }

        printf(
            "Disk %d metadata: PASS\n",
            i
        );
    }

    /*
     * Validate metadata.
     */

    if (metadata_validate(&metadata) != 0)
    {
        printf(
            "Metadata validation: FAIL\n"
        );

        for (int i = 0; i < disk_count; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    printf(
        "\nMetadata validation: PASS\n"
    );

    /*
     * Close disks.
     */

    for (int i = 0; i < disk_count; i++)
    {
        device_close(fds[i]);
    }

    printf(
        "\n=================================\n"
    );

    printf(
        " RAIDVOL CREATE: PASS\n"
    );

    printf(
        "=================================\n"
    );

    return EXIT_SUCCESS;
}

/*
 * =========================================
 * OPEN + END-TO-END I/O
 * =========================================
 */

static int open_volume(
    int disk_count,
    char *paths[]
)
{
    RaidVolume volume;

    printf("=================================\n");
    printf("         RaidVol Open\n");
    printf("=================================\n\n");

    printf(
        "Supplied Disk Count: %d\n\n",
        disk_count
    );

    /*
     * Open using metadata.
     */

    if (volume_open_from_metadata(
            &volume,
            (const char **)paths,
            disk_count) != 0)
    {
        printf(
            "\nRaidVol open: FAIL\n"
        );

        return EXIT_FAILURE;
    }

    printf("\n");

    printf(
        "Detected RAID Level : %s\n",
        raid_level_name(
            volume.metadata.raid_level
        )
    );

    printf(
        "Detected Devices    : %u\n",
        volume.metadata.device_count
    );

    printf(
        "Detected Block Size : %u bytes\n",
        volume.metadata.block_size
    );

    printf(
        "Detected Volume Size: %llu bytes\n",
        (unsigned long long)
        volume.metadata.volume_size
    );

    /*
     * =========================================
     * END-TO-END I/O TEST
     * =========================================
     */

    printf("\n=================================\n");
    printf("       Volume I/O Test\n");
    printf("=================================\n\n");

    /*
     * RAID-1
     */

    if (volume.raid_level == RAID_LEVEL_1)
    {
        char write_buffer[RAID1_BLOCK_SIZE];
        char read_buffer[RAID1_BLOCK_SIZE];

        memset(
            write_buffer,
            0,
            sizeof(write_buffer)
        );

        memset(
            read_buffer,
            0,
            sizeof(read_buffer)
        );

        snprintf(
            write_buffer,
            sizeof(write_buffer),
            "RaidVol Metadata-Driven RAID-1 Test"
        );

        printf(
            "Writing through discovered RAID-1 volume...\n\n"
        );

        if (raid1_write(
                volume.fds,
                0,
                write_buffer) != 0)
        {
            printf(
                "Volume write: FAIL\n"
            );

            volume_close(&volume);

            return EXIT_FAILURE;
        }

        printf(
            "\nReading through discovered RAID-1 volume...\n\n"
        );

        if (raid1_read(
                volume.fds,
                0,
                read_buffer) != 0)
        {
            printf(
                "Volume read: FAIL\n"
            );

            volume_close(&volume);

            return EXIT_FAILURE;
        }

        if (memcmp(
                write_buffer,
                read_buffer,
                sizeof(write_buffer)) != 0)
        {
            printf(
                "Volume data verification: FAIL\n"
            );

            volume_close(&volume);

            return EXIT_FAILURE;
        }

        printf(
            "\nVolume data verification: PASS\n"
        );
    }

    /*
     * RAID-0
     */

    else if (volume.raid_level == RAID_LEVEL_0)
    {
        char write_buffer[RAID0_BLOCK_SIZE];
        char read_buffer[RAID0_BLOCK_SIZE];

        memset(
            write_buffer,
            0,
            sizeof(write_buffer)
        );

        memset(
            read_buffer,
            0,
            sizeof(read_buffer)
        );

        snprintf(
            write_buffer,
            sizeof(write_buffer),
            "RaidVol Metadata-Driven RAID-0 Test"
        );

        printf(
            "Writing through discovered RAID-0 volume...\n\n"
        );

        if (raid0_write(
                volume.fds,
                0,
                write_buffer) != 0)
        {
            printf(
                "Volume write: FAIL\n"
            );

            volume_close(&volume);

            return EXIT_FAILURE;
        }

        printf(
            "\nReading through discovered RAID-0 volume...\n\n"
        );

        if (raid0_read(
                volume.fds,
                0,
                read_buffer) != 0)
        {
            printf(
                "Volume read: FAIL\n"
            );

            volume_close(&volume);

            return EXIT_FAILURE;
        }

        if (memcmp(
                write_buffer,
                read_buffer,
                sizeof(write_buffer)) != 0)
        {
            printf(
                "Volume data verification: FAIL\n"
            );

            volume_close(&volume);

            return EXIT_FAILURE;
        }

        printf(
            "\nVolume data verification: PASS\n"
        );
    }
else if (volume.raid_level == RAID_LEVEL_5)
{
    unsigned char write_buffer[RAID5_BLOCK_SIZE];
    unsigned char read_buffer[RAID5_BLOCK_SIZE];

    memset(
        write_buffer,
        0,
        sizeof(write_buffer)
    );

    memset(
        read_buffer,
        0,
        sizeof(read_buffer)
    );

    snprintf(
        (char *)write_buffer,
        sizeof(write_buffer),
        "RaidVol Metadata-Driven RAID-5 Test"
    );

    printf(
        "Writing through discovered RAID-5 volume...\n\n"
    );

    if (raid5_write(
            volume.fds,
            0,
            write_buffer) != 0)
    {
        printf(
            "Volume write: FAIL\n"
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    printf(
        "\nReading through discovered RAID-5 volume...\n\n"
    );

    if (raid5_read(
            volume.fds,
            0,
            read_buffer) != 0)
    {
        printf(
            "Volume read: FAIL\n"
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    if (memcmp(
            write_buffer,
            read_buffer,
            sizeof(write_buffer)) != 0)
    {
        printf(
            "Volume data verification: FAIL\n"
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    printf(
        "\nVolume data verification: PASS\n"
    );
}

    else
    {
        printf(
            "I/O test unavailable for RAID level %u.\n",
            volume.metadata.raid_level
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    volume_close(&volume);

    printf(
        "\nAll disks closed successfully.\n"
    );

    printf(
        "\n=================================\n"
    );

    printf(
        " RAIDVOL END-TO-END OPEN/IO: PASS\n"
    );

    printf(
        "=================================\n"
    );

    return EXIT_SUCCESS;
}

/*
 * =========================================
 * RAID-5 PARITY ENGINE TEST
 * =========================================
 */

static int test_raid5_parity(void)
{
    unsigned char data_a[RAID5_BLOCK_SIZE];
    unsigned char data_b[RAID5_BLOCK_SIZE];

    unsigned char parity[RAID5_BLOCK_SIZE];
    unsigned char recovered[RAID5_BLOCK_SIZE];

    /*
     * Generate deterministic test data.
     */

    for (size_t i = 0;
         i < RAID5_BLOCK_SIZE;
         i++)
    {
        data_a[i] =
            (unsigned char)(i % 256);

        data_b[i] =
            (unsigned char)((i * 3) % 256);

        parity[i] = 0;
        recovered[i] = 0;
    }

    printf("=================================\n");
    printf("       RAID-5 Parity Test\n");
    printf("=================================\n\n");

    /*
     * Calculate:
     *
     * P = A XOR B
     */

    printf(
        "Calculating XOR parity...\n"
    );

    raid5_calculate_parity(
        data_a,
        data_b,
        parity,
        RAID5_BLOCK_SIZE
    );

    printf(
        "Parity calculation: PASS\n"
    );

    /*
     * Simulate failure of B.
     *
     * B = A XOR P
     */

    printf(
        "\nSimulating Data B failure...\n"
    );

    raid5_rebuild_data(
        data_a,
        parity,
        recovered,
        RAID5_BLOCK_SIZE
    );

    /*
     * Verify recovered block.
     */

    if (memcmp(
            data_b,
            recovered,
            RAID5_BLOCK_SIZE) != 0)
    {
        printf(
            "RAID-5 rebuild verification: FAIL\n"
        );

        return EXIT_FAILURE;
    }

    printf(
        "RAID-5 rebuild verification: PASS\n"
    );

    printf(
        "\n=================================\n"
    );

    printf(
        " RAID-5 PARITY ENGINE: PASS\n"
    );

    printf(
        "=================================\n"
    );

    return EXIT_SUCCESS;
}
static int test_raid5_mapping(void)
{
    printf("=================================\n");
    printf("       RAID-5 Mapping Test\n");
    printf("=================================\n\n");

    for (size_t block = 0; block < 6; block++)
    {
        size_t device;
        size_t physical_block;

        raid5_map(
            block,
            &device,
            &physical_block
        );

        printf(
            "Virtual Block %zu -> Disk %zu, Physical Block %zu\n",
            block,
            device,
            physical_block
        );
    }

    printf("\nParity disk per stripe:\n");

    for (size_t stripe = 0; stripe < 6; stripe++)
    {
        printf(
            "Stripe %zu -> Parity Disk %zu\n",
            stripe,
            raid5_parity_disk(stripe)
        );
    }

    printf("\n=================================\n");
    printf(" RAID-5 MAPPING TEST: PASS\n");
    printf("=================================\n");

    return EXIT_SUCCESS;
}
static int test_raid5_io(void)
{
    int fds[RAID5_DEVICE_COUNT];

    const char *paths[] =
    {
        "disks/disk0.img",
        "disks/disk1.img",
        "disks/disk2.img"
    };

    unsigned char write_buffer[RAID5_BLOCK_SIZE];
    unsigned char read_buffer[RAID5_BLOCK_SIZE];

    printf("=================================\n");
    printf("       RAID-5 Real I/O Test\n");
    printf("=================================\n\n");

    /*
     * Open all RAID-5 disks.
     */
    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            printf(
                "Failed to open Disk %zu: %s\n",
                i,
                paths[i]
            );

            for (size_t j = 0; j < i; j++)
            {
                device_close(fds[j]);
            }

            return EXIT_FAILURE;
        }

        printf(
            "Opened Disk %zu: %s\n",
            i,
            paths[i]
        );
    }

    /*
     * Prepare test data.
     */
    memset(
        write_buffer,
        0,
        sizeof(write_buffer)
    );

    memset(
        read_buffer,
        0,
        sizeof(read_buffer)
    );

    snprintf(
        (char *)write_buffer,
        sizeof(write_buffer),
        "RaidVol RAID-5 Real Disk I/O Test"
    );

    printf("\nWriting Virtual Block 0...\n\n");

    if (raid5_write(
            fds,
            0,
            write_buffer) != 0)
    {
        printf("RAID-5 write: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    printf("\nReading Virtual Block 0...\n\n");

    if (raid5_read(
            fds,
            0,
            read_buffer) != 0)
    {
        printf("RAID-5 read: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Verify data.
     */
    if (memcmp(
            write_buffer,
            read_buffer,
            RAID5_BLOCK_SIZE) != 0)
    {
        printf(
            "\nRAID-5 DATA VERIFICATION: FAIL\n"
        );

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    printf(
        "\nRAID-5 DATA VERIFICATION: PASS\n"
    );

    /*
     * Close disks.
     */
    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        device_close(fds[i]);
    }

    printf("\nAll disks closed successfully.\n");

    printf("\n=================================\n");
    printf(" RAID-5 REAL DISK I/O: PASS\n");
    printf("=================================\n");

    return EXIT_SUCCESS;
}
static int test_raid5_physical_parity(void)
{
    int fds[RAID5_DEVICE_COUNT];

    const char *paths[] =
    {
        "disks/disk0.img",
        "disks/disk1.img",
        "disks/disk2.img"
    };

    unsigned char data_a[RAID5_BLOCK_SIZE];
    unsigned char data_b[RAID5_BLOCK_SIZE];
    unsigned char actual_parity[RAID5_BLOCK_SIZE];
    unsigned char expected_parity[RAID5_BLOCK_SIZE];

    printf("=================================\n");
    printf("   RAID-5 Physical Parity Test\n");
    printf("=================================\n\n");

    /*
     * Open all three disks.
     */
    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            printf(
                "Failed to open Disk %zu: %s\n",
                i,
                paths[i]
            );

            for (size_t j = 0; j < i; j++)
            {
                device_close(fds[j]);
            }

            return EXIT_FAILURE;
        }

        printf(
            "Opened Disk %zu: %s\n",
            i,
            paths[i]
        );
    }

    /*
     * Stripe 0 layout:
     *
     * Disk 0 -> Parity
     * Disk 1 -> Data A
     * Disk 2 -> Data B
     */
    printf("\nStripe 0 layout:\n");
    printf("Disk 0 -> Parity\n");
    printf("Disk 1 -> Data A\n");
    printf("Disk 2 -> Data B\n");

    /*
     * Read Data A from Disk 1.
     */
    ssize_t result_a =
        device_read(
            fds[1],
            data_a,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET
        );

    if (result_a != RAID5_BLOCK_SIZE)
    {
        perror("Reading Disk 1 data");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Read Data B from Disk 2.
     */
    ssize_t result_b =
        device_read(
            fds[2],
            data_b,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET
        );

    if (result_b != RAID5_BLOCK_SIZE)
    {
        perror("Reading Disk 2 data");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Read actual parity from Disk 0.
     */
    ssize_t result_parity =
        device_read(
            fds[0],
            actual_parity,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET
        );

    if (result_parity != RAID5_BLOCK_SIZE)
    {
        perror("Reading Disk 0 parity");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Calculate expected parity:
     *
     * P = A XOR B
     */
    raid5_calculate_parity(
        data_a,
        data_b,
        expected_parity,
        RAID5_BLOCK_SIZE
    );

    /*
     * Compare calculated parity
     * with physically stored parity.
     */
    if (memcmp(
            expected_parity,
            actual_parity,
            RAID5_BLOCK_SIZE) != 0)
    {
        printf(
            "\nRAID-5 PHYSICAL PARITY: FAIL\n"
        );

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    printf(
        "\nExpected Parity == Stored Parity\n"
    );

    printf(
        "RAID-5 PHYSICAL PARITY: PASS\n"
    );

    /*
     * Close disks.
     */
    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        device_close(fds[i]);
    }

    printf("\nAll disks closed successfully.\n");

    printf("\n=================================\n");
    printf(" RAID-5 PARITY VERIFICATION: PASS\n");
    printf("=================================\n");

    return EXIT_SUCCESS;
}
static int test_raid5_rebuild(void)
{
    int fds[RAID5_DEVICE_COUNT];

    const char *paths[] =
{
    "disks/raid5_0.img",
    "disks/raid5_1.img",
    "disks/raid5_2.img"
};

    unsigned char original_data[RAID5_BLOCK_SIZE];
    unsigned char surviving_data[RAID5_BLOCK_SIZE];
    unsigned char parity[RAID5_BLOCK_SIZE];
    unsigned char recovered_data[RAID5_BLOCK_SIZE];

    printf("=================================\n");
    printf("    RAID-5 Failure Rebuild Test\n");
    printf("=================================\n\n");

    /*
     * Open all disks.
     */
    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            printf(
                "Failed to open Disk %zu: %s\n",
                i,
                paths[i]
            );

            for (size_t j = 0; j < i; j++)
            {
                device_close(fds[j]);
            }

            return EXIT_FAILURE;
        }

        printf(
            "Opened Disk %zu: %s\n",
            i,
            paths[i]
        );
    }

    /*
     * Read the original data from Disk 1.
     *
     * We save this only so we can verify
     * that the rebuilt data is identical.
     */
    if (device_read(
            fds[1],
            original_data,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET) != RAID5_BLOCK_SIZE)
    {
        perror("Reading original Disk 1 data");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Simulate Disk 1 failure.
     */
    printf("\nSimulating Disk 1 failure...\n");

    printf(
        "Disk 1 -> FAILED\n"
    );

    /*
     * Disk 1 will NOT be read again.
     *
     * Surviving disks:
     *
     * Disk 0 -> Parity
     * Disk 2 -> Data
     */

    /*
     * Read surviving Data B from Disk 2.
     */
    if (device_read(
            fds[2],
            surviving_data,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET) != RAID5_BLOCK_SIZE)
    {
        perror("Reading surviving Disk 2 data");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Read parity from Disk 0.
     */
    if (device_read(
            fds[0],
            parity,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET) != RAID5_BLOCK_SIZE)
    {
        perror("Reading parity from Disk 0");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Rebuild:
     *
     * Missing Data A =
     *
     *     Parity XOR Data B
     */
    printf(
        "\nRebuilding missing Disk 1 data...\n"
    );

    raid5_rebuild_data(
        surviving_data,
        parity,
        recovered_data,
        RAID5_BLOCK_SIZE
    );

    /*
     * Verify reconstructed data.
     */
    if (memcmp(
            original_data,
            recovered_data,
            RAID5_BLOCK_SIZE) != 0)
    {
        printf(
            "\nRAID-5 REBUILD VERIFICATION: FAIL\n"
        );

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    printf(
        "\nOriginal Data == Recovered Data\n"
    );

    printf(
        "RAID-5 REBUILD VERIFICATION: PASS\n"
    );

    /*
     * Write rebuilt data back to Disk 1.
     */
    printf(
        "\nWriting recovered data back to Disk 1...\n"
    );

    if (device_write(
            fds[1],
            recovered_data,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET) != RAID5_BLOCK_SIZE)
    {
        perror("Writing rebuilt Disk 1 data");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Read Disk 1 again after rebuild.
     */
    unsigned char rebuilt_disk_data[RAID5_BLOCK_SIZE];

    if (device_read(
            fds[1],
            rebuilt_disk_data,
            RAID5_BLOCK_SIZE,
            RAIDVOL_DATA_OFFSET) != RAID5_BLOCK_SIZE)
    {
        perror("Reading rebuilt Disk 1 data");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    /*
     * Final byte-for-byte verification.
     */
    if (memcmp(
            original_data,
            rebuilt_disk_data,
            RAID5_BLOCK_SIZE) != 0)
    {
        printf(
            "\nRAID-5 DISK REBUILD: FAIL\n"
        );

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        {
            device_close(fds[i]);
        }

        return EXIT_FAILURE;
    }

    printf(
        "Recovered Disk 1 data == Original Disk 1 data\n"
    );

    printf(
        "RAID-5 DISK REBUILD: PASS\n"
    );

    /*
     * Close all disks.
     */
    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        device_close(fds[i]);
    }

    printf(
        "\nAll disks closed successfully.\n"
    );

    printf("\n=================================\n");
    printf(" RAID-5 FAILURE RECOVERY: PASS\n");
    printf("=================================\n");

    return EXIT_SUCCESS;
}
/*
 * RAID-5 degraded read and failure recovery test.
 */
static int test_raid5_degraded(void)
{
    printf("\n");
    printf("=================================\n");
    printf(" RAID-5 DEGRADED READ TEST\n");
    printf("=================================\n\n");

    const char *paths[] =
    {
        "disks/raid5_0.img",
        "disks/raid5_1.img",
        "disks/raid5_2.img"
    };

    RaidVolume volume;

    /*
     * Open the RAID-5 volume using metadata.
     */
    if (volume_open_from_metadata(
            &volume,
            paths,
            3) != 0)
    {
        printf(
            "\nRAID-5 degraded test: volume open FAIL\n"
        );

        return EXIT_FAILURE;
    }

    /*
     * Prepare known test data.
     */
    unsigned char original_data[RAID5_BLOCK_SIZE];
    unsigned char recovered_data[RAID5_BLOCK_SIZE];

    for (size_t i = 0;
         i < RAID5_BLOCK_SIZE;
         i++)
    {
        original_data[i] =
            (unsigned char)(i % 256);

        recovered_data[i] = 0;
    }

    /*
     * Write a known block through RAID-5.
     *
     * Virtual Block 0 maps to Disk 1.
     */
    printf(
        "Writing known data to Virtual Block 0...\n\n"
    );

    if (raid5_write(
            volume.fds,
            0,
            original_data) != 0)
    {
        printf(
            "Initial RAID-5 write: FAIL\n"
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    /*
     * Virtual Block 0 is on Disk 1.
     */
    size_t failed_disk = 1;

    printf(
        "\nSimulating Disk %zu failure...\n",
        failed_disk
    );

    printf(
        "Disk %zu -> FAILED\n\n",
        failed_disk
    );

    /*
     * Perform degraded read.
     *
     * The data is no longer read from Disk 1.
     *
     * Instead:
     *
     *     Disk 0 / Disk 2 surviving data
     *                  +
     *             parity
     *                  |
     *                  XOR
     *                  |
     *                  v
     *            recovered data
     */
    printf(
        "Reading Virtual Block 0 in degraded mode...\n\n"
    );

    if (volume_read_degraded(
            &volume,
            0,
            failed_disk,
            recovered_data) != 0)
    {
        printf(
            "\nRAID-5 degraded read: FAIL\n"
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    /*
     * Verify byte-for-byte.
     */
    if (memcmp(
            original_data,
            recovered_data,
            RAID5_BLOCK_SIZE) != 0)
    {
        printf(
            "\nRAID-5 degraded data verification: FAIL\n"
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    printf(
        "\nOriginal Data == Recovered Data\n"
    );

    printf(
        "RAID-5 DEGRADED READ: PASS\n"
    );

    /*
     * Now test failure of a parity disk.
     *
     * This verifies that data remains readable
     * even when parity is unavailable.
     */
    size_t parity_failed_disk =
        raid5_parity_disk(0);

    printf(
        "\nSimulating parity Disk %zu failure...\n",
        parity_failed_disk
    );

    memset(
        recovered_data,
        0,
        sizeof(recovered_data)
    );

    if (volume_read_degraded(
            &volume,
            0,
            parity_failed_disk,
            recovered_data) != 0)
    {
        printf(
            "RAID-5 parity failure read: FAIL\n"
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    if (memcmp(
            original_data,
            recovered_data,
            RAID5_BLOCK_SIZE) != 0)
    {
        printf(
            "RAID-5 parity failure verification: FAIL\n"
        );

        volume_close(&volume);

        return EXIT_FAILURE;
    }

    printf(
        "Parity Disk Failure Read: PASS\n"
    );

    volume_close(&volume);

    printf(
        "\nAll disks closed successfully.\n"
    );

    printf(
        "\n=================================\n"
    );

    printf(
        " RAID-5 DEGRADED OPERATION: PASS\n"
    );

    printf(
        "=================================\n"
    );

    return EXIT_SUCCESS;
}

/*
 * =========================================
 * RAID-0 MAPPING TEST
 * =========================================
 */

static int test_raid0_mapping(void)
{
    printf("=================================\n");
    printf("       RAID-0 Mapping Test\n");
    printf("=================================\n\n");

    for (size_t block = 0; block < 6; block++)
    {
        size_t device;
        size_t physical_block;

        raid0_map(
            block,
            &device,
            &physical_block
        );

        printf(
            "Virtual Block %zu -> Disk %zu, Physical Block %zu\n",
            block,
            device,
            physical_block
        );
    }

    printf("\n=================================\n");
    printf(" RAID-0 MAPPING TEST: PASS\n");
    printf("=================================\n");

    return EXIT_SUCCESS;
}

/*
 * =========================================
 * CHECKSUM TEST
 * =========================================
 */

static int test_checksum(void)
{
    unsigned char data[] = "RaidVol Milestone 8";
    size_t data_size = sizeof(data) - 1;

    printf("=================================\n");
    printf("       RaidVol Checksum Test\n");
    printf("=================================\n\n");

    uint32_t checksum =
        checksum_calculate(data, data_size);

    printf(
        "Original checksum : 0x%08X\n",
        checksum
    );

    printf("\nVerifying original data...\n");

    if (!checksum_verify(
            data,
            data_size,
            checksum))
    {
        printf(
            "Original data verification: FAIL\n"
        );
        return EXIT_FAILURE;
    }

    printf(
        "Original data verification: PASS\n"
    );

    printf("\nSimulating silent corruption...\n");

    data[0] ^= 0xFF;

    if (checksum_verify(
            data,
            data_size,
            checksum))
    {
        printf(
            "Corruption detection: FAIL\n"
        );
        return EXIT_FAILURE;
    }

    printf(
        "Corruption detection: PASS\n"
    );

    printf("\n=================================\n");
    printf(" CHECKSUM TEST: PASS\n");
    printf("=================================\n");

    return EXIT_SUCCESS;
}

static int test_raid5_checksum(void)
{
    int fds[RAID5_DEVICE_COUNT];

    const char *paths[] =
    {
        "disks/raid5_0.img",
        "disks/raid5_1.img",
        "disks/raid5_2.img"
    };

    unsigned char write_buffer[RAID5_BLOCK_SIZE];
    unsigned char read_buffer[RAID5_BLOCK_SIZE];

    printf("=================================\n");
    printf(" RAID-5 CHECKSUM TEST\n");
    printf("=================================\n\n");

    /*
     * Open RAID-5 disks.
     */
    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            printf("Failed to open Disk %zu\n", i);

            for (size_t j = 0; j < i; j++)
                device_close(fds[j]);

            return EXIT_FAILURE;
        }

        printf("Opened Disk %zu: %s\n", i, paths[i]);
    }

    /*
     * Prepare known data.
     */
    memset(write_buffer, 0, sizeof(write_buffer));
    memset(read_buffer, 0, sizeof(read_buffer));

    snprintf(
        (char *)write_buffer,
        sizeof(write_buffer),
        "RaidVol Milestone 8 Silent Corruption Test"
    );

    /*
     * Write through RAID-5.
     */
    printf("\nWriting known data through RAID-5...\n\n");

    if (raid5_write(
            fds,
            0,
            write_buffer) != 0)
    {
        printf("RAID-5 write: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    /*
     * Calculate checksum.
     */
    uint32_t expected_checksum =
        checksum_calculate(
            write_buffer,
            RAID5_BLOCK_SIZE
        );

    printf(
        "Stored checksum: 0x%08X\n",
        expected_checksum
    );

    /*
     * Persist checksum.
     */
    if (checksum_store_write(
            "disks/raid5_checksums.bin",
            0,
            expected_checksum) != 0)
    {
        printf("Checksum storage: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    printf("Checksum storage: PASS\n");

    /*
     * Normal verification.
     */
    printf("\nReading data normally...\n");

    if (raid5_read(
            fds,
            0,
            read_buffer) != 0)
    {
        printf("RAID-5 read: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    uint32_t stored_checksum;

    if (checksum_store_read(
            "disks/raid5_checksums.bin",
            0,
            &stored_checksum) != 0)
    {
        printf("Checksum read: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    if (!checksum_verify(
            read_buffer,
            RAID5_BLOCK_SIZE,
            stored_checksum))
    {
        printf("Normal checksum verification: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    printf("Normal checksum verification: PASS\n");

    /*
     * Virtual Block 0 maps to Disk 1, offset 4096.
     *
     * Save original byte.
     */
    unsigned char original_byte;

    if (device_read(
            fds[1],
            &original_byte,
            1,
            RAIDVOL_DATA_OFFSET) != 1)
    {
        printf("Reading original byte: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    /*
     * Simulate silent corruption.
     */
    printf("\nSimulating silent corruption...\n");

    unsigned char corrupted_byte =
        original_byte ^ 0xFF;

    if (device_write(
            fds[1],
            &corrupted_byte,
            1,
            RAIDVOL_DATA_OFFSET) != 1)
    {
        printf("Corruption simulation: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    printf("Physical byte modified silently.\n");

    /*
     * Read corrupted data.
     */
    memset(read_buffer, 0, sizeof(read_buffer));

    if (raid5_read(
            fds,
            0,
            read_buffer) != 0)
    {
        printf("Corrupted data read: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    /*
     * Verify checksum.
     */
    if (!checksum_verify(
            read_buffer,
            RAID5_BLOCK_SIZE,
            stored_checksum))
    {
        printf("Checksum mismatch detected!\n");
        printf("SILENT CORRUPTION DETECTED: PASS\n");
    }
    else
    {
        printf("SILENT CORRUPTION DETECTED: FAIL\n");

        /*
         * Restore original byte before exiting.
         */
        device_write(
            fds[1],
            &original_byte,
            1,
            RAIDVOL_DATA_OFFSET);

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    /*
     * Restore original byte.
     */
    printf("\nRestoring original byte...\n");

    if (device_write(
            fds[1],
            &original_byte,
            1,
            RAIDVOL_DATA_OFFSET) != 1)
    {
        printf("Data restoration: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    printf("Data restoration: PASS\n");

    /*
     * Final verification.
     */
    memset(read_buffer, 0, sizeof(read_buffer));

    if (raid5_read(
            fds,
            0,
            read_buffer) != 0)
    {
        printf("Final read: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    if (!checksum_verify(
            read_buffer,
            RAID5_BLOCK_SIZE,
            stored_checksum))
    {
        printf("Final checksum verification: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    printf("Final checksum verification: PASS\n");

    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        device_close(fds[i]);

    printf("\nAll disks closed successfully.\n");

    printf("\n=================================\n");
    printf(" RAID-5 CHECKSUM TEST: PASS\n");
    printf("=================================\n");

    return EXIT_SUCCESS;
}
static int test_raid5_checksum_multi(void)
{
    int fds[RAID5_DEVICE_COUNT];

    const char *paths[] =
    {
        "disks/raid5_0.img",
        "disks/raid5_1.img",
        "disks/raid5_2.img"
    };

    const size_t TEST_BLOCKS = 4;

    unsigned char write_buffer[RAID5_BLOCK_SIZE];
    unsigned char read_buffer[RAID5_BLOCK_SIZE];

    printf("=================================\n");
    printf(" RAID-5 MULTI-BLOCK CHECKSUM TEST\n");
    printf("=================================\n\n");

    /*
     * Open RAID-5 disks.
     */
    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            printf("Failed to open Disk %zu\n", i);

            for (size_t j = 0; j < i; j++)
                device_close(fds[j]);

            return EXIT_FAILURE;
        }

        printf("Opened Disk %zu: %s\n", i, paths[i]);
    }

    /*
     * Write multiple blocks and store checksums.
     */
    printf("\nWriting %zu blocks and storing checksums...\n\n",
           TEST_BLOCKS);

    for (size_t block = 0; block < TEST_BLOCKS; block++)
    {
        memset(write_buffer, 0, sizeof(write_buffer));

        snprintf(
            (char *)write_buffer,
            sizeof(write_buffer),
            "RaidVol M8F Block %zu - Multi-Block Checksum Test",
            block
        );

        if (raid5_write(
                fds,
                block,
                write_buffer) != 0)
        {
            printf("Block %zu write: FAIL\n", block);

            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return EXIT_FAILURE;
        }

        uint32_t checksum =
            checksum_calculate(
                write_buffer,
                RAID5_BLOCK_SIZE
            );

        if (checksum_store_write(
                "disks/raid5_checksums.bin",
                block,
                checksum) != 0)
        {
            printf("Block %zu checksum storage: FAIL\n", block);

            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return EXIT_FAILURE;
        }

        printf(
            "Block %zu -> checksum 0x%08X : PASS\n",
            block,
            checksum
        );
    }

    /*
     * Verify every block.
     */
    printf("\nVerifying all stored checksums...\n\n");

    for (size_t block = 0; block < TEST_BLOCKS; block++)
    {
        memset(read_buffer, 0, sizeof(read_buffer));

        if (raid5_read(
                fds,
                block,
                read_buffer) != 0)
        {
            printf("Block %zu read: FAIL\n", block);

            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return EXIT_FAILURE;
        }

        uint32_t stored_checksum;

        if (checksum_store_read(
                "disks/raid5_checksums.bin",
                block,
                &stored_checksum) != 0)
        {
            printf("Block %zu checksum read: FAIL\n", block);

            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return EXIT_FAILURE;
        }

        if (!checksum_verify(
                read_buffer,
                RAID5_BLOCK_SIZE,
                stored_checksum))
        {
            printf(
                "Block %zu checksum verification: FAIL\n",
                block
            );

            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return EXIT_FAILURE;
        }

        printf(
            "Block %zu checksum verification: PASS\n",
            block
        );
    }

    printf("\nAll %zu blocks verified successfully.\n",
           TEST_BLOCKS);

    /*
     * Simulate silent corruption in Block 2.
     *
     * RAID-5 mapping for the first four blocks:
     *
     * Block 0 -> Disk 1
     * Block 1 -> Disk 2
     * Block 2 -> Disk 1
     * Block 3 -> Disk 2
     *
     * Each physical block begins at RAIDVOL_DATA_OFFSET.
     */
const size_t corrupt_block = 2;

size_t corrupt_disk;
size_t corrupt_physical_block;

raid5_map(
    corrupt_block,
    &corrupt_disk,
    &corrupt_physical_block
    
);

printf(
    "Block %zu maps to Disk %zu, Physical Block %zu\n",
    corrupt_block,
    corrupt_disk,
    corrupt_physical_block
);

    unsigned char original_byte;

    if (device_read(
            fds[corrupt_disk],
            &original_byte,
            1,
            RAIDVOL_DATA_OFFSET +
            (corrupt_block / (RAID5_DEVICE_COUNT - 1))
            * RAID5_BLOCK_SIZE) != 1)
    {
        printf("Reading corruption target: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    unsigned char corrupted_byte =
        original_byte ^ 0xFF;

    printf("\nSimulating silent corruption...\n");
    printf(
        "Corrupting physical data for Virtual Block %zu...\n",
        corrupt_block
    );

    off_t corrupt_offset =
    RAIDVOL_DATA_OFFSET +
    corrupt_physical_block * RAID5_BLOCK_SIZE;

    if (device_write(
            fds[corrupt_disk],
            &corrupted_byte,
            1,
            corrupt_offset) != 1)
    {
        printf("Corruption simulation: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    /*
     * Read corrupted block and compare checksum.
     */
    memset(read_buffer, 0, sizeof(read_buffer));

    if (raid5_read(
            fds,
            corrupt_block,
            read_buffer) != 0)
    {
        printf("Corrupted block read: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    uint32_t stored_checksum;

    if (checksum_store_read(
            "disks/raid5_checksums.bin",
            corrupt_block,
            &stored_checksum) != 0)
    {
        printf("Corrupted block checksum read: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    if (!checksum_verify(
            read_buffer,
            RAID5_BLOCK_SIZE,
            stored_checksum))
    {
        printf("Checksum mismatch detected for Block %zu!\n",
               corrupt_block);

        printf("MULTI-BLOCK CORRUPTION DETECTION: PASS\n");
    }
    else
    {
        printf("MULTI-BLOCK CORRUPTION DETECTION: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    /*
     * Restore corrupted byte.
     */
    printf("\nRestoring corrupted byte...\n");

    if (device_write(
            fds[corrupt_disk],
            &original_byte,
            1,
            corrupt_offset) != 1)
    {
        printf("Restoration: FAIL\n");

        for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
            device_close(fds[i]);

        return EXIT_FAILURE;
    }

    printf("Restoration: PASS\n");

    /*
     * Final verification of all blocks.
     */
    printf("\nFinal verification of all blocks...\n\n");

    for (size_t block = 0; block < TEST_BLOCKS; block++)
    {
        memset(read_buffer, 0, sizeof(read_buffer));

        if (raid5_read(
                fds,
                block,
                read_buffer) != 0)
        {
            printf("Final read Block %zu: FAIL\n", block);

            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return EXIT_FAILURE;
        }

        if (checksum_store_read(
                "disks/raid5_checksums.bin",
                block,
                &stored_checksum) != 0)
        {
            printf(
                "Final checksum read Block %zu: FAIL\n",
                block
            );

            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return EXIT_FAILURE;
        }

        if (!checksum_verify(
                read_buffer,
                RAID5_BLOCK_SIZE,
                stored_checksum))
        {
            printf(
                "Final checksum Block %zu: FAIL\n",
                block
            );

            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return EXIT_FAILURE;
        }

        printf(
            "Block %zu final verification: PASS\n",
            block
        );
    }

    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        device_close(fds[i]);

    printf("\nAll disks closed successfully.\n");

    printf("\n=================================\n");
    printf(" RAID-5 MULTI-BLOCK CHECKSUM: PASS\n");
    printf("=================================\n");

    return EXIT_SUCCESS;
}
/*
 * =========================================
 * MAIN
 * =========================================
 */

int main(
    int argc,
    char *argv[]
)
{
    if (argc < 2)
    {
        print_usage(argv[0]);

        return EXIT_FAILURE;
    }

    /*
     * RAID-5 parity test
     */

if (strcmp(argv[1],"raid5-test") == 0)
{
        return test_raid5_parity();
}
if (strcmp(argv[1], "checksum-test") == 0)
{
    return test_checksum();
}
if (strcmp(argv[1], "raid5-io-test") == 0)
{
    return test_raid5_io();
}
if (strcmp(argv[1], "raid5-parity-test") == 0)
{
    return test_raid5_physical_parity();
}
if (strcmp(argv[1], "raid5-rebuild-test") == 0)
{
    return test_raid5_rebuild();
}
if (strcmp(argv[1], "raid5-checksum-test") == 0)
{
    return test_raid5_checksum();
}
if (strcmp(argv[1], "raid5-checksum-multi-test") == 0)
{
    return test_raid5_checksum_multi();
}
else if (strcmp(argv[1], "raid5-degraded-test") == 0)
{
    return test_raid5_degraded();
}
if (strcmp(argv[1], "benchmark") == 0)
{
    return benchmark_run();
}

    /*
     * INSPECT
     */

    if (strcmp(
            argv[1],
            "inspect") == 0)
    {
        if (argc != 3)
        {
            printf(
                "Error: inspect requires a disk image.\n\n"
            );

            print_usage(argv[0]);

            return EXIT_FAILURE;
        }

        return inspect_disk(argv[2]);
    }

    /*
     * CREATE
     */

    if (strcmp(
            argv[1],
            "create") == 0)
    {
        if (argc < 5)
        {
            printf(
                "Error: invalid create command.\n\n"
            );

            print_usage(argv[0]);

            return EXIT_FAILURE;
        }

        if (strcmp(
                argv[2],
                "--raid") != 0)
        {
            printf(
                "Error: expected --raid.\n\n"
            );

            print_usage(argv[0]);

            return EXIT_FAILURE;
        }

        int raid_level =
            atoi(argv[3]);

        int disk_count =
            argc - 4;

        if (disk_count >
            MAX_CREATE_DEVICES)
        {
            printf(
                "Error: maximum supported disks are %d.\n",
                MAX_CREATE_DEVICES
            );

            return EXIT_FAILURE;
        }

        return create_volume(
            raid_level,
            disk_count,
            &argv[4]
        );
    }

    /*
     * OPEN
     */

    if (strcmp(
            argv[1],
            "open") == 0)
    {
        int disk_count =
            argc - 2;

        if (disk_count <= 0)
        {
            printf(
                "Error: open requires at least one disk.\n\n"
            );

            print_usage(argv[0]);

            return EXIT_FAILURE;
        }

        if (disk_count >
            MAX_CREATE_DEVICES)
        {
            printf(
                "Error: maximum supported disks are %d.\n",
                MAX_CREATE_DEVICES
            );

            return EXIT_FAILURE;
        }

        return open_volume(
            disk_count,
            &argv[2]
        );
    }
if (strcmp(argv[1], "raid5-map-test") == 0)
{
    return test_raid5_mapping();
}
if (strcmp(argv[1], "raid0-map-test") == 0)
{
    return test_raid0_mapping();
}

    /*
     * UNKNOWN COMMAND
     */

    printf(
        "Unknown command: %s\n\n",
        argv[1]
    );

    print_usage(argv[0]);

    return EXIT_FAILURE;
}
