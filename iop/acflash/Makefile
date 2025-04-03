# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_SRC_DIR = src/
IOP_INC_DIR = $(IOP_SRC_DIR)

IOP_BIN = acflash.irx
IOP_OBJS = \
	acflash-entry.o \
	flash.o \
	i28f640j5.o \
	mbm29f033c.o \
	imports.o \
	exports.o

all: $(IOP_BIN)
	cp $< ..

clean:
	rm -f -r $(IOP_OBJS) $(IOP_BIN)

ifeq ($(DEBUG), 1)
  $(info -- debug enabled)
  IOP_CFLAGS += -DDEBUG=1
endif

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal