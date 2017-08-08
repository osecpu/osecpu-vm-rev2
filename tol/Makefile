TARGETS=osectols

ROOT_PATH=..
include ../common.mak

default:
	make $(TARGETS)

osectols:
	$(CC_NATIVE) -o osectols osectols.c

clean:
	-$(REMOVE) $(TARGETS)
