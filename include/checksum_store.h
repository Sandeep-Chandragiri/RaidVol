#ifndef CHECKSUM_STORE_H
#define CHECKSUM_STORE_H

#include <stdint.h>
#include <stddef.h>

int checksum_store_write(
    const char *path,
    size_t block,
    uint32_t checksum
);

int checksum_store_read(
    const char *path,
    size_t block,
    uint32_t *checksum
);

#endif
