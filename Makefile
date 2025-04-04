#     ____                    _        ____
#    |  _ \  ___  _ __   __ _| | ___  |  _ \ _   _ _ __ ___  _ __   ___ _ __
#    | | | |/ _ \| '_ \ / _` | |/ _ \ | | | | | | | '_ ` _ \| '_ \ / _ \ '__|
#    | |_| | (_) | | | | (_| | |  __/ | |_| | |_| | | | | | | |_) |  __/ |
#    |____/ \___/|_| |_|\__, |_|\___| |____/ \__,_|_| |_| |_| .__/ \___|_|
#                       |___/                               |_|
# security dongle dumper for PlayStation2 based namco system 246/256

EE_BIN = ACFLASH_DUMPER.ELF
EE_OBJS_DIR = embed/
EE_OBJS = main.o modelname.o ioprp.o pad.o \
	$(addprefix $(EE_OBJS_DIR), usbd.o bdm.o bdmfs_fatfs.o usbmass_bd.o fileXio.o iomanX.o acflash.o acflash_fs.o)

DEBUG ?= 1
EE_CFLAGS += -fdata-sections -ffunction-sections -DNEWLIB_PORT_AWARE
EE_LDFLAGS += -Wl,--gc-sections
EE_LIBS += -liopreboot -ldebug -lpatches -lfileXio -lcdvd -lpadx


ifeq ($(DEBUG), 1)
  EXCEPTION_HANDLER = 1
  EE_CFLAGS += -DDEBUG -O0 -g
else
  EE_CFLAGS += -Os
  EE_LDFLAGS += -s
endif
ifeq ($(EXCEPTION_HANDLER),1)
  EE_CFLAGS += -DCATCH_EXCEPTIONS
  EE_LIBS += -Lexceptionman -lexceptionman
endif
all: $(EE_BIN)

clean:
	rm -rf $(EE_OBJS) $(EE_BIN)
	$(MAKE) -C iop/  clean 

ioprp.img:
	wget https://github.com/israpps/wLaunchELF_ISR/raw/system-2x6-support/iop/__precompiled/IOPRP_FILEIO.IMG -O $@

%.c: %.img
	bin2c $< $@ ioprp

iop/acflash_fs.irx: iop/acflash_fs/
	$(MAKE) -C $<
iop/acflash.irx: iop/acflash/
	$(MAKE) -C $<

vpath %.irx iop/
vpath %.irx $(PS2SDK)/iop/irx/
IRXTAG = $(notdir $(addsuffix _irx, $(basename $<)))
$(EE_OBJS_DIR)%.c: %.irx
	$(DIR_GUARD)
	@bin2c $< $@ $(IRXTAG)

# Include makefiles
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal