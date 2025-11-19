#ifndef ACSRAMDMP_RPC
#define ACSRAMDMP_RPC
#include <stdint.h>
#define ACSRAM_DUMP_MAXCHUNK_SIZE 0x800
struct DumpSram
{
    int32_t result;
    uint32_t size;
    uint32_t off;
    uint8_t buffer[ACSRAM_DUMP_MAXCHUNK_SIZE];
};

enum {
    ACSRAM_RPC_IRX = 0xac5,
    ACSRAM_DUMP_CHUNK = 0xb00b5,
};

#endif