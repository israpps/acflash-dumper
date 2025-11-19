#include "irx_imports.h"
#include <errno.h>
#include "acsram_dumper.h"
#include <stdint.h>

#define FUNLOG() DPRINTF("%s\n", __func__)
#define DPRINTF(format, args...) printf(MODNAME ": " format, ##args) // module printf

#define MODNAME "acsram_dumper"
IRX_ID(MODNAME, 1, 1);

static SifRpcDataQueue_t rpcqueue;
static SifRpcServerData_t serverdata;
static int rpcthid;
static unsigned char SifServerBuffer[0x1000];

#define SRAM_SIZE 0x8000
static void *cmdHandler(int cmd, void *buffer, int nbytes)
{
    printf("%s: CMD %d w buff of %d bytes\n", __FUNCTION__, cmd, nbytes);
    int RET;

    switch (cmd)
    {
    case ACSRAM_DUMP_CHUNK:
        struct DumpSram* Params = buffer;
        printf("acSramRead(0x%x, %p, 0x%x)\n", Params->off, Params->buffer, Params->size);
        Params->result = acSramRead(Params->off, Params->buffer, Params->size);
        break;
    default:
        DPRINTF("INVALID CMD 0x%X\n");
        break;
    }
    return buffer;
}


static void SifSecrDownloadHeaderThread(void *parameters)
{
    (void)parameters;
    printf("ACSRAM RPC THREAD RUNNING\n");

    if (!sceSifCheckInit()) {
        DPRINTF("sceSifCheckInit 0. initializing SIF\n");
        sceSifInit();
    }

    sceSifInitRpc(0);
    sceSifSetRpcQueue(&rpcqueue, GetThreadId());
    sceSifRegisterRpc(&serverdata, ACSRAM_RPC_IRX, &cmdHandler, SifServerBuffer, NULL, NULL, &rpcqueue);
    sceSifRpcLoop(&rpcqueue);
}

int _start(int argc, char** argv) {
    printf("%s start\n", MODNAME);
    iop_thread_t thread;
    thread.attr      = TH_C;
    thread.priority  = 0x28;
    thread.stacksize = 0x1000;

    thread.thread = &SifSecrDownloadHeaderThread;
    if ((rpcthid = CreateThread(&thread)) == 0) {
        return MODULE_NO_RESIDENT_END;
    }
    StartThread(rpcthid, NULL);


    return MODULE_RESIDENT_END;
}
