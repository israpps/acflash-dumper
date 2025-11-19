
#include <stdbool.h>
#include <tamtypes.h>
#include <string.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include "acsram_dumper.h"

#define DPRINTF(fmt, x...) printf(fmt, ##x)
#define CHECK_RPC_INIT() if (!rpc_initialized) {DPRINTF("ERROR: Cannot call %s if RPC server is not initialized\n", __FUNCTION__); return -2;}


static SifRpcClientData_t AcSRAMRPC;
static int rpc_initialized = false;


int acsram_dumper_init(void)
{
    printf("binding to ACSRAM_RPC_IRX (%d) service\n", ACSRAM_RPC_IRX);
    int retries = 100;
    if (rpc_initialized)
        {return 0;}

    int E;
	while(retries--)
	{
		if((E = SifBindRpc(&AcSRAMRPC, ACSRAM_RPC_IRX, 0)) < 0)
        {
            DPRINTF("Failed to bind RPC server for ACSRAM DUMPER (%d)\n", E);
			return SCE_EBINDMISS;
        }

		if(AcSRAMRPC.server != NULL)
			break;

		nopdelay();
	}

	rpc_initialized = retries;
    printf("%sbound at %d retries", (retries) ? "" : "NOT ", 100-retries);
	return (retries) ? 0 : ESRCH;
}

#define RPC_BUFPARAM(x) &x, sizeof(x)

int dump_sram(void* buf, uint32_t readsize, uint32_t off) {
    CHECK_RPC_INIT();
    printf("dumping SRAM...\n");
    struct DumpSram pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.size = readsize;
    pkt.off = off;
    if (SifCallRpc(&AcSRAMRPC, ACSRAM_DUMP_CHUNK, 0, RPC_BUFPARAM(pkt), RPC_BUFPARAM(pkt), NULL, NULL) < 0)
    {
        DPRINTF("%s: RPC ERROR\n", __FUNCTION__);
        return -SCE_ECALLMISS;
    }
    printf("readed %d bytes of sram at offset %d\n", pkt.result, off);
    if (pkt.result == readsize && buf != NULL) memcpy(buf, pkt.buffer, readsize); //copy back the kilobyte from RPC to the original pointer, kbit and kc changed
    else printf("%s: not copying result, error on RPC or output buf is NULL (%d|%d)\n", __func__, pkt.result == readsize, buf != NULL);
    return pkt.result;
}