#define _POSIX_C_SOURCE 200809L

#include "checksum_store.h"

#include <fcntl.h>
#include <unistd.h>

int checksum_store_write(
    const char *path,
    size_t block,
    uint32_t checksum
)
{
    int fd = open(path, O_RDWR | O_CREAT, 0644);

    if (fd < 0)
        return -1;

    off_t offset = (off_t)(block * sizeof(uint32_t));

    ssize_t result = pwrite(
        fd,
        &checksum,
        sizeof(checksum),
        offset
    );

    close(fd);

    return result == sizeof(checksum) ? 0 : -1;
}

int checksum_store_read(
    const char *path,
    size_t block,
    uint32_t *checksum
)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0)
        return -1;

    off_t offset = (off_t)(block * sizeof(uint32_t));

    ssize_t result = pread(
        fd,
        checksum,
        sizeof(*checksum),
        offset
    );

    close(fd);

    return result == sizeof(*checksum) ? 0 : -1;
}
