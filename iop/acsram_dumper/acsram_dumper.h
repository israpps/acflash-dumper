
#include "acsram_rpc.h"
#include <errno.h>

int acsram_dumper_init(void);
int dump_sram(void* buf, uint32_t readsize, uint32_t off);