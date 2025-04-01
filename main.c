/***
 *        ___   ______________    ___   _____ __  __   ____  __  ____  _______  __________ 
 *       /   | / ____/ ____/ /   /   | / ___// / / /  / __ \/ / / /  |/  / __ \/ ____/ __ \
 *      / /| |/ /   / /_  / /   / /| | \__ \/ /_/ /  / / / / / / / /|_/ / /_/ / __/ / /_/ /
 *     / ___ / /___/ __/ / /___/ ___ |___/ / __  /  / /_/ / /_/ / /  / / ____/ /___/ _, _/ 
 *    /_/  |_\____/_/   /_____/_/  |_/____/_/ /_/  /_____/\____/_/  /_/_/   /_____/_/ |_|  
 *        
 *  Namco System 2x6 FLASH dumper
 *  Copyright (c) 2025 Matias Israelson - MIT license                                                                   
 */

 #ifdef CATCH_EXCEPTIONS
 #include "exceptionman/exceptions.h"
 #endif
#include <kernel.h>
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
#include "acflash_common.h"
int dumpflash(const char* path);
const char *ModelNameGet(void);
void scr_centerputs(const char* buf, char fillerbyte);
void scr_fillhalf(int size, char filler);
int ModelNameInit(void);
uint16_t getConsoleID();

const char* flashdump_path = "mass:/2X6_FLASH_DUMP.BIN";

typedef struct {
    int id;
    int ret;
} modinfo_t;
modinfo_t sio2man, mcman, mcserv, padman, usbd, bdm, fatfs, usbmass, acflash, acflash_fs, fileXio, iomanX, sec_checker;

#define EXTERN_MODULE(_irx) extern unsigned char _irx[]; extern unsigned int size_##_irx
EXTERN_MODULE(ioprp);
EXTERN_MODULE(acflash_irx);
EXTERN_MODULE(acflash_fs_irx);
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

char ROMVER[15];
int loadmodules();

struct acflashinfo {
    int bsize;
    int bcount;
    int sizebytes;
} acflash_status = {0,0,0};

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
    scr_centerputs(" Namco System 246/256 flash dumper ", '=');
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
    //if (!loadusb()) goto tosleep;
    
    iomanX.id = LOADMODULE(iomanX_irx, &iomanX.ret);
    INFORM(iomanX);
    fileXio.id = LOADMODULE(fileXio_irx, &fileXio.ret);
    INFORM(fileXio);
    if (MODULE_OK(fileXio.id, fileXio.ret)) {
        fileXioInit();
    } else {
        scr_printf("\tFailed to load fileXio. aborting dump...\n");
        goto tosleep;
    }
    if (loadmodules() == 0) {
        scr_setfontcolor(0xffffff);
        dumpflash("acflash:");
    }
    sleep(120);
    
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
    if (ret != 0) {
        scr_printf("\t- error: 'mass:/' not found (%d)\n", ret);
        return 0;
    } else {
        scr_printf("\t- found 'mass:/' after %d attempt%c\n", retries, (retries > 1) ? 's' : ' ');
        return 1;
    }
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
    PadInitPads();
    sec_checker.id = LOADMODULEFILE("rom0:DAEMON", &sec_checker.ret);
    INFORM(sec_checker);
    if (!MODULE_OK(sec_checker.id, sec_checker.ret)) {
        scr_printf("\tfailed to load rom0:DAEMON!!!\n");
        return -1;
    }


    acflash.id =  LOADMODULE(acflash_irx, &acflash.ret); // modified genvmc module that auths card with I_McDetectCard2. this should reset the watchdog before begining dump
    INFORM(acflash);
    if (!MODULE_OK(acflash.id, acflash.ret)) {
        if (acflash.ret == -16) scr_printf("\n\tACFLASH.IRX: Invalid FLASH Status\n");
        if (acflash.ret == -06) scr_printf("\n\tACFLASH.IRX: FLASH Not found\n");
        return -1;
    }
    acflash_fs.id =  LOADMODULE(acflash_fs_irx, &acflash_fs.ret); // modified genvmc module that auths card with I_McDetectCard2. this should reset the watchdog before begining dump
    INFORM(acflash_fs);
    if (!MODULE_OK(acflash_fs.id, acflash_fs.ret)) {
        return -1;
    }
    acflash_status.bcount = fileXioDevctl("acflash:", ACFLASH_FS_GET_BLOCKCONT, NULL, 0, NULL, 0);
    acflash_status.bsize = fileXioDevctl("acflash:", ACFLASH_FS_GET_BLOCKSIZE, NULL, 0, NULL, 0);
    acflash_status.sizebytes = (acflash_status.bsize*acflash_status.bcount);
    scr_printf("\tFLASH: Block size:%d Blocks:%d (%d Bytes)\n", acflash_status.bsize, acflash_status.bcount, (acflash_status.bsize*acflash_status.bcount));
    if (!acflash_status.bcount || !acflash_status.bsize) {scr_printf("\tError: invalid sizes in flash?\n"); return -1;}
    if (acflash_status.sizebytes <= 0) {scr_printf("\tError: invalid sizes in flash?\n");return -1;}
    return 0;
}

int dumpflash(const char* path) {
    int ret = 0, infd, outfd, written = 0, readed = 0;
    uint8_t *FLASHBUF = malloc(acflash_status.sizebytes);
    if (FLASHBUF == NULL) {
        scr_setfontcolor(0x0000FF);
        scr_printf("\tError: Could not Allocate %d bytes for reading the flash\n", acflash_status.sizebytes);
        return -ENOMEM;
    }
    infd = open(path, O_RDONLY);
    if (infd > 0) {
        scr_printf("\t- Opened '%s:' now reading\n", path);
        readed = read(infd, FLASHBUF, acflash_status.sizebytes);
        if (readed != acflash_status.sizebytes) {
            scr_setfontcolor(0x0000FF);
            scr_printf("\tError: Could not read whole flash size (readed:%d | Size:%d)\n", readed, acflash_status.sizebytes);
            ret = -EIO;
        } else {
            scr_printf("\t- Readed %d Bytes from the flash\n", readed);
            if (!strncmp("RESET", FLASHBUF, sizeof("RESET"))) {
                scr_setfontcolor(0x00FFFF);
                scr_printf("\t- INFO: ROMFS 'RESET' magic found at begining of FLASH\n");
                scr_setfontcolor(0xFFFFFF);
            }
            scr_printf("\t- Opening output file '%s'\n", flashdump_path);
            outfd = open(flashdump_path, O_WRONLY | O_CREAT | O_TRUNC);
            if (outfd > 0) {
                written = write(outfd, FLASHBUF, acflash_status.sizebytes);
                if (written != acflash_status.sizebytes) {
                    scr_setfontcolor(0x0000FF);
                    scr_printf("\tError: Could not Write whole flash size to USB (written:%d | Size:%d)\n", written, acflash_status.sizebytes);
                    ret = -EIO;
                } else {
                    scr_printf("\t- SUCCESS: Written whole flash to file\n");
                    close(outfd);
                }
            } else {
                scr_setfontcolor(0x0000FF);
                scr_printf("\tError: Could Not open file for dumping flash\n");
                ret = outfd;
            }
        }
        close(infd);
    } else {ret = infd; scr_printf("\tFailed to open 'acflash'\n");}
    if (FLASHBUF) free(FLASHBUF);
    return ret;
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
    sio_puts("# ACFLASH dumper start\n# BuilDate: "__DATE__ " " __TIME__ "\n");
    while (!SifIopRebootBuffer(ioprp, size_ioprp)) {}; //replace FILEIO
    while (!SifIopSync()) {};
    SifLoadStartModule("rom0:CDVDFSV", 0, NULL, NULL);
    SifLoadStartModule("rom0:ACDEV", 0, NULL, NULL); // just in case
}
