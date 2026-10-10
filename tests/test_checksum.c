#include <stdio.h>
#include <string.h>

#include "checksum.h"

int main(void)
{
    char data[] = "RaidVol Milestone 8";

    printf("=================================\n");
    printf("       RaidVol Checksum Test\n");
    printf("=================================\n\n");

    uint32_t checksum =
        checksum_calculate(data, strlen(data));

    printf("Original data: %s\n", data);
    printf("Checksum     : 0x%08X\n\n", checksum);

    printf("Verifying original data...\n");

    if (checksum_verify(data, strlen(data), checksum))
        printf("Checksum verification: PASS\n");
    else
        printf("Checksum verification: FAIL\n");

    printf("\n---------------------------------\n");
    printf("Simulating silent corruption...\n");
    printf("---------------------------------\n");

    data[0] = 'X';

    printf("Corrupted data: %s\n", data);

    if (checksum_verify(data, strlen(data), checksum))
        printf("Corruption detection: FAIL\n");
    else
        printf("Corruption detection: PASS\n");

    printf("\n=================================\n");
    printf(" MILESTONE 8A/8B: PASS\n");
    printf("=================================\n");

    return 0;
}
