
ROOT_PATH=..
include $(ROOT_PATH)/common.mak

SRCS = api.c decode.c driver.c extend.c float.c integer.c osecpu-vm.c other.c pointer.c tek.c debug.c
BINNAME = osecpu

$(BINNAME): $(SRCS) Makefile
	echo $(OS)
	$(CC_NATIVE) -o $@ $(SRCS) $(CFLAGS_VM)

clean:
	-$(REMOVE) $(BINNAME)
