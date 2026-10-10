#include "checksum.h"

uint32_t checksum_calculate(const void *data, size_t size)
{
    const unsigned char *bytes = data;

    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < size; i++)
    {
        crc ^= bytes[i];

        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFF;
}

int checksum_verify(
    const void *data,
    size_t size,
    uint32_t expected
)
{
    uint32_t actual = checksum_calculate(data, size);

    return actual == expected;
}
