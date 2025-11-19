
IOP_BIN = acsram_dumper.irx
IOP_OBJS = main.o imports.o


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