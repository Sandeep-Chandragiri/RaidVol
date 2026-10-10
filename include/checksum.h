#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

uint32_t checksum_calculate(
    const void *data,
    size_t size
);

int checksum_verify(
    const void *data,
    size_t size,
    uint32_t expected
);

#endif
