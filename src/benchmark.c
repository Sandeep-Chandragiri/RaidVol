#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "benchmark.h"
#include "device.h"
#include "raid0.h"
#include "raid1.h"
#include "raid5.h"

#define BENCHMARK_BLOCK_SIZE 4096
#define BENCHMARK_BLOCKS 256
#define BENCHMARK_TOTAL_BYTES \
    ((size_t)BENCHMARK_BLOCK_SIZE * BENCHMARK_BLOCKS)

#define SINGLE_DISK "disks/disk0.img"

static double elapsed_seconds(
    struct timespec start,
    struct timespec end
)
{
    return (double)(end.tv_sec - start.tv_sec)
         + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

static void fill_buffer(
    unsigned char *buffer,
    size_t size,
    size_t block
)
{
    for (size_t i = 0; i < size; i++)
        buffer[i] = (unsigned char)((i + block) % 256);
}

/*
 * Single-disk benchmark.
 */
static int benchmark_single_disk(
    double *write_mb,
    double *read_mb
)
{
    int fd = device_open(SINGLE_DISK);

    if (fd < 0)
        return -1;

    unsigned char buffer[BENCHMARK_BLOCK_SIZE];

    struct timespec start;
    struct timespec end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t block = 0; block < BENCHMARK_BLOCKS; block++)
    {
        fill_buffer(buffer, sizeof(buffer), block);

        off_t offset =
            4096 + block * BENCHMARK_BLOCK_SIZE;

        if (device_write(
                fd,
                buffer,
                BENCHMARK_BLOCK_SIZE,
                offset) != BENCHMARK_BLOCK_SIZE)
        {
            device_close(fd);
            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double write_time = elapsed_seconds(start, end);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t block = 0; block < BENCHMARK_BLOCKS; block++)
    {
        off_t offset =
            4096 + block * BENCHMARK_BLOCK_SIZE;

        if (device_read(
                fd,
                buffer,
                BENCHMARK_BLOCK_SIZE,
                offset) != BENCHMARK_BLOCK_SIZE)
        {
            device_close(fd);
            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double read_time = elapsed_seconds(start, end);

    *write_mb =
        ((double)BENCHMARK_TOTAL_BYTES / (1024.0 * 1024.0))
        / write_time;

    *read_mb =
        ((double)BENCHMARK_TOTAL_BYTES / (1024.0 * 1024.0))
        / read_time;

    device_close(fd);

    return 0;
}

/*
 * RAID-0 benchmark.
 */
static int benchmark_raid0(
    double *write_mb,
    double *read_mb
)
{
    const char *paths[] =
    {
        "disks/raid0_0.img",
        "disks/raid0_1.img",
        "disks/raid0_2.img"
    };

    int fds[RAID0_DEVICE_COUNT];

    for (size_t i = 0; i < RAID0_DEVICE_COUNT; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            for (size_t j = 0; j < i; j++)
                device_close(fds[j]);

            return -1;
        }
    }

    unsigned char buffer[RAID0_BLOCK_SIZE];

    struct timespec start;
    struct timespec end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t block = 0; block < BENCHMARK_BLOCKS; block++)
    {
        fill_buffer(buffer, sizeof(buffer), block);

        if (raid0_write(fds, block, buffer) != 0)
        {
            for (size_t i = 0; i < RAID0_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double write_time = elapsed_seconds(start, end);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t block = 0; block < BENCHMARK_BLOCKS; block++)
    {
        if (raid0_read(fds, block, buffer) != 0)
        {
            for (size_t i = 0; i < RAID0_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double read_time = elapsed_seconds(start, end);

    *write_mb =
        ((double)BENCHMARK_TOTAL_BYTES / (1024.0 * 1024.0))
        / write_time;

    *read_mb =
        ((double)BENCHMARK_TOTAL_BYTES / (1024.0 * 1024.0))
        / read_time;

    for (size_t i = 0; i < RAID0_DEVICE_COUNT; i++)
        device_close(fds[i]);

    return 0;
}

/*
 * RAID-1 benchmark.
 */
static int benchmark_raid1(
    double *write_mb,
    double *read_mb
)
{
    const char *paths[] =
    {
        "disks/disk0.img",
        "disks/disk1.img"
    };

    int fds[RAID1_DEVICE_COUNT];

    for (size_t i = 0; i < RAID1_DEVICE_COUNT; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            for (size_t j = 0; j < i; j++)
                device_close(fds[j]);

            return -1;
        }
    }

    unsigned char buffer[RAID1_BLOCK_SIZE];

    struct timespec start;
    struct timespec end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t block = 0; block < BENCHMARK_BLOCKS; block++)
    {
        fill_buffer(buffer, sizeof(buffer), block);

        if (raid1_write(fds, block, buffer) != 0)
        {
            for (size_t i = 0; i < RAID1_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double write_time = elapsed_seconds(start, end);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t block = 0; block < BENCHMARK_BLOCKS; block++)
    {
        if (raid1_read(fds, block, buffer) != 0)
        {
            for (size_t i = 0; i < RAID1_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double read_time = elapsed_seconds(start, end);

    *write_mb =
        ((double)BENCHMARK_TOTAL_BYTES / (1024.0 * 1024.0))
        / write_time;

    *read_mb =
        ((double)BENCHMARK_TOTAL_BYTES / (1024.0 * 1024.0))
        / read_time;

    for (size_t i = 0; i < RAID1_DEVICE_COUNT; i++)
        device_close(fds[i]);

    return 0;
}

/*
 * RAID-5 benchmark.
 */
static int benchmark_raid5(
    double *write_mb,
    double *read_mb
)
{
    const char *paths[] =
    {
        "disks/raid5_0.img",
        "disks/raid5_1.img",
        "disks/raid5_2.img"
    };

    int fds[RAID5_DEVICE_COUNT];

    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
    {
        fds[i] = device_open(paths[i]);

        if (fds[i] < 0)
        {
            for (size_t j = 0; j < i; j++)
                device_close(fds[j]);

            return -1;
        }
    }

    unsigned char buffer[RAID5_BLOCK_SIZE];

    struct timespec start;
    struct timespec end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t block = 0; block < BENCHMARK_BLOCKS; block++)
    {
        fill_buffer(buffer, sizeof(buffer), block);

        if (raid5_write(fds, block, buffer) != 0)
        {
            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double write_time = elapsed_seconds(start, end);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t block = 0; block < BENCHMARK_BLOCKS; block++)
    {
        if (raid5_read(fds, block, buffer) != 0)
        {
            for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
                device_close(fds[i]);

            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double read_time = elapsed_seconds(start, end);

    *write_mb =
        ((double)BENCHMARK_TOTAL_BYTES / (1024.0 * 1024.0))
        / write_time;

    *read_mb =
        ((double)BENCHMARK_TOTAL_BYTES / (1024.0 * 1024.0))
        / read_time;

    for (size_t i = 0; i < RAID5_DEVICE_COUNT; i++)
        device_close(fds[i]);

    return 0;
}

int benchmark_run(void)
{
    double single_write;
    double single_read;

    double raid0_write_speed;
    double raid0_read_speed;

    double raid1_write_speed;
    double raid1_read_speed;

    double raid5_write_speed;
    double raid5_read_speed;

    printf("=========================================\n");
    printf("       RAIDVOL PERFORMANCE BENCHMARK\n");
    printf("=========================================\n\n");

    printf("Block size : %d bytes\n", BENCHMARK_BLOCK_SIZE);
    printf("Blocks     : %d\n", BENCHMARK_BLOCKS);
    printf("Total data : %.2f MB\n\n",
           (double)BENCHMARK_TOTAL_BYTES /
           (1024.0 * 1024.0));

    printf("Running Single Disk benchmark...\n");

    if (benchmark_single_disk(
            &single_write,
            &single_read) != 0)
    {
        printf("Single Disk benchmark: FAIL\n");
        return EXIT_FAILURE;
    }

    printf("Single Disk: PASS\n");

    printf("Running RAID-0 benchmark...\n");

    if (benchmark_raid0(
            &raid0_write_speed,
            &raid0_read_speed) != 0)
    {
        printf("RAID-0 benchmark: FAIL\n");
        return EXIT_FAILURE;
    }

    printf("RAID-0: PASS\n");

    printf("Running RAID-1 benchmark...\n");

    if (benchmark_raid1(
            &raid1_write_speed,
            &raid1_read_speed) != 0)
    {
        printf("RAID-1 benchmark: FAIL\n");
        return EXIT_FAILURE;
    }

    printf("RAID-1: PASS\n");

    printf("Running RAID-5 benchmark...\n");

    if (benchmark_raid5(
            &raid5_write_speed,
            &raid5_read_speed) != 0)
    {
        printf("RAID-5 benchmark: FAIL\n");
        return EXIT_FAILURE;
    }

    printf("RAID-5: PASS\n");

    printf("\n=========================================\n");
    printf("          BENCHMARK RESULTS\n");
    printf("=========================================\n");

    printf(
        "%-15s %12s %12s\n",
        "Configuration",
        "Write MB/s",
        "Read MB/s"
    );

    printf("-----------------------------------------\n");

    printf(
        "%-15s %12.2f %12.2f\n",
        "Single Disk",
        single_write,
        single_read
    );

    printf(
        "%-15s %12.2f %12.2f\n",
        "RAID-0",
        raid0_write_speed,
        raid0_read_speed
    );

    printf(
        "%-15s %12.2f %12.2f\n",
        "RAID-1",
        raid1_write_speed,
        raid1_read_speed
    );

    printf(
        "%-15s %12.2f %12.2f\n",
        "RAID-5",
        raid5_write_speed,
        raid5_read_speed
    );

    printf("=========================================\n");
    printf(" RAIDVOL PERFORMANCE BENCHMARK: PASS\n");
    printf("=========================================\n");

    return EXIT_SUCCESS;
}
