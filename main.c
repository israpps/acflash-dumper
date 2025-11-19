/**
 *     ___________  ___  ___  ___  ______ _   ____  _________ ___________ 
 *    /  ___| ___ \/ _ \ |  \/  |  |  _  \ | | |  \/  || ___ \  ___| ___ \
 *    \ `--.| |_/ / /_\ \| .  . |  | | | | | | | .  . || |_/ / |__ | |_/ /
 *     `--. \    /|  _  || |\/| |  | | | | | | | |\/| ||  __/|  __||    / 
 *    /\__/ / |\ \| | | || |  | |  | |/ /| |_| | |  | || |   | |___| |\ \ 
 *    \____/\_| \_\_| |_/\_|  |_/  |___/  \___/\_|  |_/\_|   \____/\_| \_|
 *                                                                        
 *  Namco System 2x6 SRAM dumper
 *  Copyright (c) 2025 Matias Israelson - MIT license    
 *                                                                        
 */

#ifdef CATCH_EXCEPTIONS
#include "exceptionman/exceptions.h"
#endif
#include <sifrpc-common.h>
#include <kernel.h>
#include <fcntl.h>
#include <stdio.h>
#include <iopheap.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <rom0_info.h>
#include <fileio.h>
#include <fileXio_rpc.h>
#include <iopcontrol.h>
#include <iopcontrol_special.h>
#include <loadfile.h>
#include <sio.h>
#include <debug.h>
#include <sbv_patches.h>
#include <ps2sdkapi.h>
#include <string.h>
#include <sys/stat.h>
#include "pad.h"
#include "iop/acsram_dumper/acsram_dumper.h"

#define SRAM_SIZE 0x8000
uint8_t SRAM[SRAM_SIZE];

void genericgauge (float progress);
void genericgaugepercent(int percent);
void genericgaugepercentcalc(int curr, int max);

const char *ModelNameGet(void);
void hexdump(const void* data, uint32_t size, int hdr);
void scr_centerputs(const char* buf, char fillerbyte);
void scr_fillhalf(int size, char filler);
int ModelNameInit(void);
uint16_t getConsoleID();

typedef struct {
    int id;
    int ret;
} modinfo_t;
modinfo_t mmceman, sio2man, mcman, mcserv, padman, usbd, bdm, fatfs, usbmass, fileXio, iomanX, sec_checker, acsram, acsram_dumper;

#define EXTERN_MODULE(_irx) extern unsigned char _irx[]; extern unsigned int size_##_irx
EXTERN_MODULE(ioprp);
EXTERN_MODULE(mmceman_irx);
EXTERN_MODULE(acsram_dumper_irx);
EXTERN_MODULE(acsram_irx);
EXTERN_MODULE(usbd_irx);
EXTERN_MODULE(bdm_irx);
EXTERN_MODULE(bdmfs_fatfs_irx);
EXTERN_MODULE(usbmass_bd_irx);
EXTERN_MODULE(genvmc_irx);
EXTERN_MODULE(fileXio_irx);
EXTERN_MODULE(iomanX_irx);
#define LOADMODULE(_irx, ret) SifExecModuleBuffer(&_irx, size_##_irx, 0, NULL, ret)
#define LOADMODULEFILE(path, ret) SifLoadStartModule(path, 0, NULL, ret)
#define MODULE_OK(id, ret) (id >= 0 && ret == 0)
#define INFORM(x) scr_setfontcolor(MODULE_OK(x.id, x.ret) ? 0x00cc00 : 0x0000cc);scr_printf("\t %s: id:%d ret:%d %s", #x, x.id, x.ret, MODULE_OK(x.id, x.ret) ? "(OK)\r" : "(ERR)\n")
int loadusb();
int dumpsram(const char* path);
int checkfile(const char* path);

char ROMVER[15];
int loadmodules();

int main(int argc, char** argv) {
    sio_puts("# startup services");
    SifInitIopHeap(); // Initialize SIF services for loading modules and files.
    SifLoadFileInit();
    fioInit();
    init_scr();
    scr_setCursor(0);
    sio_puts("# pull romver");
    memset(ROMVER, 0, sizeof(ROMVER));
    GetRomName(ROMVER);
    scr_printf("\n\n");
    scr_centerputs(" Namco System 246/256 SRAM dumper ", '=');
    scr_centerputs(" Coded by El_isra ", ' ');
    scr_centerputs(" https://github.com/israpps/acflash-dumper ", ' ');
    scr_printf("\tROMVER:        %s\n", ROMVER);
    ModelNameInit();
    scr_printf("\tConsole model: %s\n", ModelNameGet());
    scr_printf("\tConsole ID:    0x%x\n", getConsoleID());
    scr_printf("\tMachineType:   %04i\n", MachineType());
    sbv_patch_enable_lmb(); // patch modload to support SifExecModuleBuffer
    sbv_patch_disable_prefix_check(); // remove security from MODLOAD

    if (!(ROMVER[4] == 'T' && ROMVER[5] == 'Z')) {
        scr_setfontcolor(0x0000FF);
        scr_printf("\tthis PS2 is NOT an arcade unit.\n\taborting...\n");
        goto tosleep;
    }
    if (!loadusb()) goto tosleep;
    iomanX.id = LOADMODULE(iomanX_irx, &iomanX.ret);
    INFORM(iomanX);
    fileXio.id = LOADMODULE(fileXio_irx, &fileXio.ret);
    INFORM(fileXio);
    if (MODULE_OK(fileXio.id, fileXio.ret)) {
        fileXioInit();
    } else {
        //scr_printf("\tFailed to load fileXio. aborting dump...\n");
        //goto tosleep;
    }
    if (loadmodules() == 0) {
        scr_setfontcolor(0xffffff);
        dumpsram("sram.bin");
    } else printf("error on loadmodules()\n");
    scr_printf("\n");
    for (int i = 80; i > 0; i--, sleep(1))
        scr_printf("\t>>> Program terminated: exiting in %d\r", i);

    
#ifdef CATCH_EXCEPTIONS
restoreExceptionHandlers();
#endif
    return 0;
tosleep:

#ifdef CATCH_EXCEPTIONS
restoreExceptionHandlers();
#endif
    SleepThread();
}
int loadusb() {
    usbd.id = LOADMODULE(usbd_irx, &usbd.ret);
    INFORM(usbd);
    bdm.id = LOADMODULE(bdm_irx, &bdm.ret);
    INFORM(bdm);
    fatfs.id = LOADMODULE(bdmfs_fatfs_irx, &fatfs.ret);
    INFORM(fatfs);
    usbmass.id = LOADMODULE(usbmass_bd_irx, &usbmass.ret);
    INFORM(usbmass);
    sleep(3);
    scr_setfontcolor(0xdddddd);
    
    struct stat buffer;
    int ret = -1;
    int retries = 0;

    while (ret != 0 && retries <= 50) {
        ret = stat("mass:/", &buffer);
        /* Wait until the device is ready */
        nopdelay();

        retries++;
    }
#ifdef STRICT_MASS_AVAILABILITY
    if (ret != 0) {
        scr_printf("\t- error: 'mass:/' not found (%d)\n", ret);
        return 0;
    } else {
        scr_printf("\t- found 'mass:/' after %d attempt%c\n", retries, (retries > 1) ? 's' : ' ');
        return 1;
    }
#else
    return 1;
#endif
}

int loadmodules() {
    sio2man.id = LOADMODULEFILE("rom0:SIO2MAN", &sio2man.ret);
    INFORM(sio2man);
    if (!MODULE_OK(sio2man.id, sio2man.ret)) {
        return -1;
    }
    mcman.id = LOADMODULEFILE("rom0:MCMAN", &mcman.ret);
    INFORM(mcman);
    if (!MODULE_OK(mcman.id, mcman.ret)) {
        return -1;
    }
    mcserv.id = LOADMODULEFILE("rom0:MCSERV", &mcserv.ret);
    INFORM(mcserv);
    if (!MODULE_OK(mcserv.id, mcserv.ret)) {
        return -1;
    }
    padman.id = LOADMODULEFILE("rom0:PADMAN", &padman.ret);
    INFORM(padman);
    if (!MODULE_OK(padman.id, padman.ret)) {
        return -1;
    }
    //PadInitPads();
    sec_checker.id = LOADMODULEFILE("rom0:DAEMON", &sec_checker.ret);
    INFORM(sec_checker);
    if (!MODULE_OK(sec_checker.id, sec_checker.ret)) {
        scr_printf("\tfailed to load rom0:DAEMON!!!\n");
        return -1;
    }
    mmceman.id = LOADMODULE(mmceman_irx, &mmceman.ret);
    INFORM(mmceman);
    acsram.id = LOADMODULE(acsram_irx, &acsram.ret);
    INFORM(acsram);
    acsram_dumper.id = LOADMODULE(acsram_dumper_irx, &acsram_dumper.ret);
    INFORM(acsram_dumper);
    if (acsram_dumper_init() != 0) {
        scr_printf("\tCannot bind RPC service to acsram_dumper.irx\n");
        return -1;
    }

    return 0;
}

int dumpsram(const char* path) {
    scr_printf("\n\t>>> reading SRAM...\n");
    printf("\n\tdumping SRAM to '%s'\n", path);
    int r, fd;
    int readed = 0, written;
    while (readed < SRAM_SIZE)
    {
        readed += dump_sram(&SRAM[readed], ACSRAM_DUMP_MAXCHUNK_SIZE, readed);
        scr_printf("  %04X", readed);
        genericgaugepercentcalc(readed, SRAM_SIZE);
        sleep(1);
    }
    scr_printf("\n\tSRAM Read complete\n");
    
#ifndef NO_HEXDUMP
    hexdump(SRAM, sizeof(SRAM), 1);
#endif
    fd = open(path, O_WRONLY|O_CREAT|O_TRUNC);
    scr_printf("\tdumping SRAM to file '%s'\n", path);
    if (fd > 0) {
        written = write(fd, SRAM, SRAM_SIZE);
        if (written != SRAM_SIZE) {
            scr_printf("\tI/O ERROR: written 0x%X bytes to file instead of 0x%X\n", written, SRAM_SIZE);
            r = -1;
        } else {
            scr_printf("\tfile dumped!\n\tremember: running this program again overwrites the dump\n");
            r = 0;
        }
        close(fd);
    } else {
        scr_printf("\tI/O ERROR: Could not open '%s'\n", path);
        r = -1;
    }
    return r;
}

void scr_fillhalf(int size, char filler) {
    for (int x=0; x<(80-size)/2;x++) scr_printf("%c", filler);
}

void scr_centerputs(const char* buf, char fillerbyte) {
    scr_fillhalf(strlen(buf), fillerbyte);
    scr_printf("%s", buf);
    scr_fillhalf(strlen(buf), fillerbyte);
    if(strlen(buf) % 2 != 0) scr_printf("\n");
}

void _ps2sdk_memory_init() {
#ifdef CATCH_EXCEPTIONS
    installExceptionHandlers();
#endif
    sio_puts("# SRAM dumper start\n# BuilDate: "__DATE__ " " __TIME__ "\n");
    while (!SifIopRebootBuffer(ioprp, size_ioprp)) {}; //replace FILEIO to avoid RPC hang
    memset(SRAM, 0, SRAM_SIZE); //between the IOPRP reboot and IOP Sync we can do things on the EE to save time, while we wait for reboot finish
    while (!SifIopSync()) {};
    SifLoadStartModule("rom0:CDVDFSV", 0, NULL, NULL); // bring back CDVDMAN RPC to avoid hangs on libcglue library
}


int checkfile(const char* path) {
    int fd = open(path, O_RDONLY);
    int r = (fd > 0);
    if (r) close(fd);
    return r;
}


void hexdump(const void* data, uint32_t size, int hdr) {
    char ascii[17];
    uint32_t i, j;
    ascii[16] = '\0';
    if (hdr) {
        for (i = 0; i < 16; i++) {
            if (i == 8)
                printf(" ");
            printf("%02X ", i);
        }
        printf("\n");
        for (i = 0; i < 23; i++)
            printf("---");
        printf("\n");
    }

    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*) data)[i]);
        if (((unsigned char*) data)[i] >= ' ' && ((unsigned char*) data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*) data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i + 1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

void genericgaugepercentcalc(int curr, int max) {
    genericgaugepercent((curr*100/(float)max));
}
void genericgaugepercent(int percent) {
    genericgauge(percent*0.01);
}

void genericgauge (float progress) {
    int barWidth = 70;

    scr_printf("[");
    int pos = barWidth * progress;
    for (int i = 0; i < barWidth; ++i)
    {
      if (i < pos)
        scr_printf("=");
      else if (i == pos)
        scr_printf(">");
      else
        scr_printf(" ");
    }

    scr_printf("]\r");
}